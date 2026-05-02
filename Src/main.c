#include "MVM.h"
#include "MVM_Device.h"
#include "MVM_Internal.h"
#include "MVM_Trace.h"
#include "MVM_VmgpDebug.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_STEPS_DEFAULT              (100000000U)
#define MAX_LOGGED_CALLS_DEFAULT       (100000U)
#define VM_MAX_STEPS_PER_HOST_FRAME    (10000U)
#define HOST_MIN_LOOP_DELAY_MS         (1U)
#define INPUT_REPEAT_DELAY_MS          (180U)
#define INPUT_REPEAT_INTERVAL_MS       (90U)
#define MVM_KEY_UP                     (0x00000001U)
#define MVM_KEY_DOWN                   (0x00000002U)
#define MVM_KEY_LEFT                   (0x00000004U)
#define MVM_KEY_RIGHT                  (0x00000008U)
#define MVM_KEY_FIRE                   (0x00000010U)
#define MVM_KEY_SELECT                 (0x00000020U)
#define MVM_POINTER_DOWN               (0x00000040U)
#define MVM_POINTER_ALTDOWN            (0x00000080U)
#define MVM_KEY_FIRE2                  (0x00000100U)
#define SDL_BACKEND_BUTTON_COUNT       (9U)

/*
 * Desktop key mapping aligned with the official Mophun SDK emulator:
 * - Up Arrow    -> KEY_UP
 * - Down Arrow  -> KEY_DOWN
 * - Left Arrow  -> KEY_LEFT
 * - Right Arrow -> KEY_RIGHT
 * - Left/Right Shift/Ctrl -> KEY_FIRE
 * - Backspace/Enter  -> KEY_SELECT
 * - Space / Keypad Enter -> KEY_FIRE2
 * - Numeric keypad 1/3/7/9 -> diagonal direction combinations
 * - Numeric keypad 2/4/6/8 -> down/left/right/up
 */

/**
 * @brief Stores the minimal SDL host backend state used by the example runner.
 */
typedef struct SdlBackend
{
  SDL_Window *window;
  SDL_Renderer *renderer;
  uint32_t width;
  uint32_t height;
  uint32_t raw_button_state;
  uint32_t pulse_button_state;
  uint32_t next_repeat_ms[SDL_BACKEND_BUTTON_COUNT];
  uint32_t last_frame_serial;
} SdlBackend;

/**
 * @brief Mirrors one minimal guest SPRITE header layout for SDL rendering.
 */
typedef struct VmSpriteHeader
{
  uint8_t palindex;
  uint8_t format;
  int16_t center_x;
  int16_t center_y;
  uint16_t width;
  uint16_t height;
  uint32_t data_addr;
  uint8_t legacy_layout;
} VmSpriteHeader;

static const uint32_t MVM_lSdlButtonMasks[SDL_BACKEND_BUTTON_COUNT] =
{
  MVM_KEY_UP,
  MVM_KEY_DOWN,
  MVM_KEY_LEFT,
  MVM_KEY_RIGHT,
  MVM_KEY_FIRE,
  MVM_KEY_SELECT,
  MVM_POINTER_DOWN,
  MVM_POINTER_ALTDOWN,
  MVM_KEY_FIRE2
};

static uint32_t sprite_format_bits_per_pixel(uint8_t format);

/**
 * @brief Reads one packed guest sprite pixel using the LSB-first packing used by the reference renderers.
 */
static uint32_t read_packed_sprite_pixel(const uint8_t *data, uint32_t pixel_index, uint32_t bits_per_pixel)
{
  uint32_t byte_index;
  uint32_t shift;
  uint32_t mask;

  if (!data || bits_per_pixel == 0u || bits_per_pixel > 8u)
  {
    return 0u;
  }

  if (bits_per_pixel == 8u)
  {
    return data[pixel_index];
  }

  byte_index = (pixel_index * bits_per_pixel) >> 3;
  shift = (pixel_index & ((8u / bits_per_pixel) - 1u)) * bits_per_pixel;
  mask = (1u << bits_per_pixel) - 1u;
  return (uint32_t)((data[byte_index] >> shift) & mask);
}

/**
 * @brief Reads one packed font pixel from tightly packed glyph data.
 */
static uint32_t read_tight_font_pixel(const uint8_t *data,
                                      uint32_t pixel_index,
                                      uint32_t bits_per_pixel,
                                      int msb_first)
{
  uint32_t pixels_per_byte;
  uint32_t byte_index;
  uint32_t local_index;
  uint32_t shift;
  uint32_t mask;

  if (!data || bits_per_pixel == 0u || bits_per_pixel > 8u)
  {
    return 0u;
  }

  if (!msb_first)
  {
    return read_packed_sprite_pixel(data, pixel_index, bits_per_pixel);
  }

  if (bits_per_pixel == 8u)
  {
    return data[pixel_index];
  }

  pixels_per_byte = 8u / bits_per_pixel;
  byte_index = pixel_index / pixels_per_byte;
  local_index = pixel_index % pixels_per_byte;
  shift = (pixels_per_byte - 1u - local_index) * bits_per_pixel;
  mask = (1u << bits_per_pixel) - 1u;
  return (uint32_t)((data[byte_index] >> shift) & mask);
}

/**
 * @brief Scores one legacy sprite-layout candidate by the density of non-zero pixels.
 */
static uint32_t score_legacy_sprite_candidate(const VMGPContext *ctx, const VmSpriteHeader *sprite)
{
  uint32_t bits_per_pixel;
  uint32_t pixel_count;
  uint32_t byte_count;
  uint32_t index;
  uint32_t pixel_index;
  uint32_t non_zero_count;
  uint32_t max_width;
  uint32_t max_height;

  if (!ctx || !sprite || sprite->width == 0u || sprite->height == 0u)
  {
    return 0u;
  }

  max_width = 128u;
  max_height = 128u;
  if (ctx->device_profile)
  {
    if (ctx->device_profile->screen_width != 0u)
    {
      max_width = ctx->device_profile->screen_width;
    }

    if (ctx->device_profile->screen_height != 0u)
    {
      max_height = ctx->device_profile->screen_height;
    }
  }

  if ((uint32_t)sprite->width > max_width || (uint32_t)sprite->height > max_height)
  {
    return 0u;
  }

  bits_per_pixel = sprite_format_bits_per_pixel(sprite->format);
  if (bits_per_pixel == 0u)
  {
    return 0u;
  }

  pixel_count = (uint32_t)sprite->width * (uint32_t)sprite->height;
  byte_count = (pixel_count * bits_per_pixel + 7u) / 8u;
  if (pixel_count == 0u || !MVM_RuntimeMemRangeOk(ctx, sprite->data_addr, byte_count))
  {
    return 0u;
  }

  non_zero_count = 0u;
  for (index = 0u; index < pixel_count; ++index)
  {
    pixel_index = read_packed_sprite_pixel(ctx->mem + sprite->data_addr, index, bits_per_pixel);

    if (pixel_index != 0u)
    {
      ++non_zero_count;
    }
  }

  return (non_zero_count * 1024u) / pixel_count;
}

/**
 * @brief Mirrors one minimal guest FONT header layout for SDL text rendering.
 */
typedef struct VmFontHeader
{
  uint32_t font_data_addr;
  uint32_t char_table_addr;
  uint8_t bpp;
  uint8_t width;
  uint8_t height;
  uint8_t palindex;
} VmFontHeader;

/**
 * @brief Returns the bits-per-pixel for one guest sprite format or zero when unsupported.
 */
static uint32_t sprite_format_bits_per_pixel(uint8_t format)
{
  switch (format)
  {
    case 0x00u:
    case 0x03u:
      return 1u;

    case 0x01u:
    case 0x04u:
      return 2u;

    case 0x02u:
    case 0x05u:
      return 4u;

    case 0x06u:
    case 0x07u:
      return 8u;

    default:
      return 0u;
  }
}

/**
 * @brief Reads one candidate guest FONT header from one absolute VM address.
 */
static int read_font_header_at(const VMGPContext *ctx, uint32_t font_addr, VmFontHeader *font)
{
  if (!ctx || !font || !MVM_RuntimeMemRangeOk(ctx, font_addr, 12u))
  {
    return 0;
  }

  font->font_data_addr = vm_read_u32_le(ctx->mem + font_addr + 0u);
  font->char_table_addr = vm_read_u32_le(ctx->mem + font_addr + 4u);
  font->bpp = ctx->mem[font_addr + 8u];
  font->width = ctx->mem[font_addr + 9u];
  font->height = ctx->mem[font_addr + 10u];
  font->palindex = ctx->mem[font_addr + 11u];

  return 1;
}

/**
 * @brief Returns non-zero when one decoded guest FONT header looks plausible.
 */
static int font_header_plausible(const VMGPContext *ctx, const VmFontHeader *font)
{
  uint32_t bits_per_char;
  uint32_t bytes_per_char;

  if (!ctx || !font)
  {
    return 0;
  }

  if ((font->bpp != 1u && font->bpp != 2u) ||
      font->width == 0u || font->height == 0u ||
      font->width > 64u || font->height > 64u)
  {
    return 0;
  }

  bits_per_char = (uint32_t)font->width * (uint32_t)font->height * (uint32_t)font->bpp;
  bytes_per_char = (bits_per_char + 7u) / 8u;
  if (bytes_per_char == 0u)
  {
    return 0;
  }

  if (!MVM_RuntimeMemRangeOk(ctx, font->font_data_addr, bytes_per_char))
  {
    return 0;
  }

  if (!MVM_RuntimeMemRangeOk(ctx, font->char_table_addr, 256u))
  {
    return 0;
  }

  return 1;
}

/**
 * @brief Describes parsed command-line options for the VM runner.
 */
typedef struct AppOptions
{
  const char *image_path;
  const char *profile_name;
  uint32_t max_steps;
  uint32_t max_logged_calls;
} AppOptions;

/**
 * @brief Describes one file-backed image source.
 */
typedef struct FileImageSource
{
  FILE *file;
  size_t size;
} FileImageSource;

/**
 * @brief Returns the selected built-in device profile or the default one.
 */
static const MpnDevProfile_t *resolve_device_profile(const char *profile_name)
{
  if (profile_name)
  {
    return MVM_FindDevProfileByName(profile_name);
  }

  if (MVM_GetDevProfileCount() == 0u)
  {
    return NULL;
  }

  return MVM_GetDevProfile(0u);
}

/**
 * @brief Converts one guest-encoded color value to 8-bit RGB.
 */
static void decode_guest_color(uint32_t color, uint8_t *red, uint8_t *green, uint8_t *blue)
{
  if ((color & 0x80000000u) != 0u || color > 0xFFu)
  {
    *red = (uint8_t)((((color & 0xFFFFu) >> 10) & 0x1Fu) * 255u / 31u);
    *green = (uint8_t)((((color & 0xFFFFu) >> 5) & 0x1Fu) * 255u / 31u);
    *blue = (uint8_t)(((color & 0xFFFFu) & 0x1Fu) * 255u / 31u);
  }
  else
  {
    *red = (uint8_t)(((color >> 5) & 0x07u) * 255u / 7u);
    *green = (uint8_t)(((color >> 2) & 0x07u) * 255u / 7u);
    *blue = (uint8_t)((color & 0x03u) * 255u / 3u);
  }
}

/**
 * @brief Applies one guest-encoded color to the active SDL renderer.
 */
static void set_renderer_guest_color(SDL_Renderer *renderer, uint32_t color)
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  if (!renderer)
  {
    return;
  }

  decode_guest_color(color, &red, &green, &blue);
  SDL_SetRenderDrawColor(renderer, red, green, blue, 255u);
}

/**
 * @brief Reads one guest SPRITE header for the minimal SDL renderer.
 */
static int read_guest_sprite_header(const VMGPContext *ctx, uint32_t sprite_addr, VmSpriteHeader *sprite)
{
  VmSpriteHeader best_candidate;
  VmSpriteHeader candidate;
  uint16_t word0;
  uint16_t word1;
  uint16_t word2;
  uint16_t word3;
  uint16_t word4;
  uint32_t best_score;
  uint32_t score;
  bool word2_looks_like_offset;

  if (!ctx || !sprite || !MVM_RuntimeMemRangeOk(ctx, sprite_addr, 14u))
  {
    return 0;
  }

  sprite->palindex = ctx->mem[sprite_addr + 0u];
  sprite->format = ctx->mem[sprite_addr + 1u];
  sprite->center_x = (int16_t)vm_read_u16_le(ctx->mem + sprite_addr + 2u);
  sprite->center_y = (int16_t)vm_read_u16_le(ctx->mem + sprite_addr + 4u);
  sprite->width = vm_read_u16_le(ctx->mem + sprite_addr + 6u);
  sprite->height = vm_read_u16_le(ctx->mem + sprite_addr + 8u);
  sprite->data_addr = sprite_addr + 10u;
  sprite->legacy_layout = 0u;

  if (sprite->width != 0u && sprite->height != 0u)
  {
    return 1;
  }

  if (!MVM_RuntimeMemRangeOk(ctx, sprite_addr, 10u))
  {
    return 1;
  }

  word0 = vm_read_u16_le(ctx->mem + sprite_addr + 0u);
  word1 = vm_read_u16_le(ctx->mem + sprite_addr + 2u);
  word2 = vm_read_u16_le(ctx->mem + sprite_addr + 4u);
  word3 = vm_read_u16_le(ctx->mem + sprite_addr + 6u);
  word4 = vm_read_u16_le(ctx->mem + sprite_addr + 8u);
  word2_looks_like_offset = (word0 != 0u) &&
                            (word4 != 0u) &&
                            (word2 >= 10u) &&
                            MVM_RuntimeMemRangeOk(ctx, sprite_addr + word2, 8u);

  best_score = 0u;
  memset(&best_candidate, 0, sizeof(best_candidate));

  if (word0 != 0u && word4 != 0u)
  {
    memset(&candidate, 0, sizeof(candidate));
    candidate.palindex = 0u;
    candidate.format = (uint8_t)(word1 & 0xFFu);
    candidate.center_x = (int16_t)word2;
    candidate.center_y = (int16_t)word3;
    candidate.width = word0;
    candidate.height = word4;
    candidate.data_addr = sprite_addr + 10u;
    candidate.legacy_layout = 1u;
    score = score_legacy_sprite_candidate(ctx, &candidate);
    if (score > best_score)
    {
      best_score = score;
      best_candidate = candidate;
    }
  }

  if (word0 != 0u && word4 != 0u && word2 != 0u)
  {
    memset(&candidate, 0, sizeof(candidate));
    candidate.palindex = 0u;
    candidate.format = (uint8_t)(word1 & 0xFFu);
    candidate.center_x = 0;
    candidate.center_y = (int16_t)word3;
    candidate.width = word0;
    candidate.height = word4;
    candidate.data_addr = sprite_addr + word2;
    candidate.legacy_layout = 2u;
    score = score_legacy_sprite_candidate(ctx, &candidate);
    if (score != 0u && word2_looks_like_offset)
    {
      score += 2048u;
    }
    if (score > best_score)
    {
      best_score = score;
      best_candidate = candidate;
    }

    memset(&candidate, 0, sizeof(candidate));
    candidate.palindex = 0u;
    candidate.format = (uint8_t)(word1 & 0xFFu);
    candidate.center_x = 0;
    candidate.center_y = (int16_t)word3;
    candidate.width = word0;
    candidate.height = word4;
    candidate.data_addr = sprite_addr + 10u + word2;
    candidate.legacy_layout = 3u;
    score = score_legacy_sprite_candidate(ctx, &candidate);
    if (score != 0u && word2_looks_like_offset)
    {
      score += 2048u;
    }
    if (score > best_score)
    {
      best_score = score;
      best_candidate = candidate;
    }
  }

  if (word2 != 0u && word4 != 0u && !word2_looks_like_offset)
  {
    memset(&candidate, 0, sizeof(candidate));
    candidate.palindex = 0u;
    candidate.format = (uint8_t)(word1 & 0xFFu);
    candidate.center_x = (int16_t)word0;
    candidate.center_y = (int16_t)word3;
    candidate.width = word2;
    candidate.height = word4;
    candidate.data_addr = sprite_addr + 10u;
    candidate.legacy_layout = 1u;
    score = score_legacy_sprite_candidate(ctx, &candidate);
    if (score > best_score)
    {
      best_score = score;
      best_candidate = candidate;
    }
  }

  if (word4 != 0u && word2 != 0u)
  {
    memset(&candidate, 0, sizeof(candidate));
    candidate.palindex = 0u;
    candidate.format = (uint8_t)(word1 & 0xFFu);
    candidate.center_x = (int16_t)word0;
    candidate.center_y = (int16_t)word3;
    candidate.width = word4;
    candidate.height = word2;
    candidate.data_addr = sprite_addr + 10u;
    candidate.legacy_layout = 1u;
    score = score_legacy_sprite_candidate(ctx, &candidate);
    if (score > best_score)
    {
      best_score = score;
      best_candidate = candidate;
    }
  }

  if (best_score != 0u)
  {
    *sprite = best_candidate;
  }

  return 1;
}

/**
 * @brief Reads one guest FONT header for the minimal SDL renderer.
 */
static int read_guest_font_header(const VMGPContext *ctx, uint32_t font_addr, VmFontHeader *font)
{
  uint32_t indirect_addr;
  VmFontHeader candidate;
  VmFontHeader direct_candidate;

  if (!ctx || !font)
  {
    return 0;
  }

  if (!read_font_header_at(ctx, font_addr, &candidate))
  {
    return 0;
  }

  if (font_header_plausible(ctx, &candidate))
  {
    *font = candidate;
    return 1;
  }

  direct_candidate = candidate;
  indirect_addr = candidate.font_data_addr;
  if (indirect_addr != 0u &&
      indirect_addr != font_addr &&
      read_font_header_at(ctx, indirect_addr, &candidate) &&
      font_header_plausible(ctx, &candidate))
  {
    *font = candidate;
    return 1;
  }

  *font = direct_candidate;

  return 1;
}

/**
 * @brief Draws one guest SPRITE using a minimal RGB332 software path.
 */
static int draw_guest_sprite(SDL_Renderer *renderer, const VMGPContext *ctx, const MVM_DrawCommand_t *command)
{
  VmSpriteHeader sprite;
  uint32_t byte_index;
  uint32_t pixels_per_byte;
  uint32_t pixel_index;
  uint32_t pixel_count;
  uint32_t index;
  uint32_t data_addr;
  int32_t x;
  int32_t y;
  uint8_t pixel;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint32_t bits_per_pixel;

  if (!renderer || !ctx || !command)
  {
    return 0;
  }

  if (!read_guest_sprite_header(ctx, command->aux, &sprite))
  {
    return 0;
  }

  if (sprite.legacy_layout != 0u)
  {
    return 0;
  }

  pixel_count = (uint32_t)sprite.width * (uint32_t)sprite.height;
  data_addr = sprite.data_addr;
  pixels_per_byte = 0u;
  bits_per_pixel = 0u;
  switch (sprite.format)
  {
    case 0x00u:
    case 0x03u:
      pixels_per_byte = 8u;
      bits_per_pixel = 1u;
      break;

    case 0x01u:
    case 0x04u:
      pixels_per_byte = 4u;
      bits_per_pixel = 2u;
      break;

    case 0x02u:
    case 0x05u:
      pixels_per_byte = 2u;
      bits_per_pixel = 4u;
      break;

    case 0x06u:
    case 0x07u:
      pixels_per_byte = 1u;
      bits_per_pixel = 8u;
      break;

    default:
      return 0;
  }

  byte_index = (pixel_count + pixels_per_byte - 1u) / pixels_per_byte;
  if (!MVM_RuntimeMemRangeOk(ctx, data_addr, byte_index))
  {
    return 0;
  }

  for (index = 0u; index < pixel_count; ++index)
  {
    pixel_index = read_packed_sprite_pixel(ctx->mem + data_addr, index, bits_per_pixel);

    if (pixel_index == 0u)
    {
      continue;
    }

    switch (sprite.format)
    {
      case 0x00u:
        red = pixel_index ? 255u : 0u;
        green = red;
        blue = red;
        break;

      case 0x01u:
        if (sprite.legacy_layout != 0u)
        {
          pixel = (uint8_t)pixel_index;
          decode_guest_color(ctx->palette_entries[pixel], &red, &green, &blue);
        }
        else
        {
          red = (uint8_t)(pixel_index * 85u);
          green = red;
          blue = red;
        }
        break;

      case 0x02u:
        red = (uint8_t)(pixel_index * 17u);
        green = red;
        blue = red;
        break;

      case 0x03u:
      case 0x04u:
      case 0x05u:
      case 0x06u:
        pixel = (uint8_t)(sprite.palindex + pixel_index);
        if (pixel >= 256u)
        {
          continue;
        }
        decode_guest_color(ctx->palette_entries[pixel], &red, &green, &blue);
        break;

      case 0x07u:
        pixel = (uint8_t)pixel_index;
        red = (uint8_t)(((pixel >> 5) & 0x07u) * 255u / 7u);
        green = (uint8_t)(((pixel >> 2) & 0x07u) * 255u / 7u);
        blue = (uint8_t)((pixel & 0x03u) * 255u / 3u);
        break;

      default:
        return 0;
    }

    x = command->x0 + (int32_t)(index % sprite.width);
    y = command->y0 + (int32_t)(index / sprite.width);

    SDL_SetRenderDrawColor(renderer, red, green, blue, 255u);
    SDL_RenderDrawPoint(renderer, x, y);
  }

  return 1;
}

/**
 * @brief Returns one tiny built-in 5x7 debug glyph for ASCII fallback text rendering.
 */
static const uint8_t *debug_font_glyph(uint8_t ch)
{
  static const uint8_t GLYPH_SPACE[7] = { 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
  static const uint8_t GLYPH_DOT[7]   = { 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0Cu, 0x0Cu };
  static const uint8_t GLYPH_COLON[7] = { 0x00u, 0x0Cu, 0x0Cu, 0x00u, 0x0Cu, 0x0Cu, 0x00u };
  static const uint8_t GLYPH_DASH[7]  = { 0x00u, 0x00u, 0x00u, 0x1Eu, 0x00u, 0x00u, 0x00u };
  static const uint8_t GLYPH_EXCL[7]  = { 0x0Cu, 0x0Cu, 0x0Cu, 0x0Cu, 0x0Cu, 0x00u, 0x0Cu };
  static const uint8_t GLYPH_0[7]     = { 0x0Eu, 0x11u, 0x13u, 0x15u, 0x19u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_1[7]     = { 0x04u, 0x0Cu, 0x04u, 0x04u, 0x04u, 0x04u, 0x0Eu };
  static const uint8_t GLYPH_2[7]     = { 0x0Eu, 0x11u, 0x01u, 0x02u, 0x04u, 0x08u, 0x1Fu };
  static const uint8_t GLYPH_3[7]     = { 0x1Eu, 0x01u, 0x01u, 0x0Eu, 0x01u, 0x01u, 0x1Eu };
  static const uint8_t GLYPH_4[7]     = { 0x02u, 0x06u, 0x0Au, 0x12u, 0x1Fu, 0x02u, 0x02u };
  static const uint8_t GLYPH_5[7]     = { 0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x01u, 0x01u, 0x1Eu };
  static const uint8_t GLYPH_6[7]     = { 0x0Eu, 0x10u, 0x10u, 0x1Eu, 0x11u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_7[7]     = { 0x1Fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x08u, 0x08u };
  static const uint8_t GLYPH_8[7]     = { 0x0Eu, 0x11u, 0x11u, 0x0Eu, 0x11u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_9[7]     = { 0x0Eu, 0x11u, 0x11u, 0x0Fu, 0x01u, 0x01u, 0x0Eu };
  static const uint8_t GLYPH_A[7]     = { 0x0Eu, 0x11u, 0x11u, 0x1Fu, 0x11u, 0x11u, 0x11u };
  static const uint8_t GLYPH_B[7]     = { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x11u, 0x11u, 0x1Eu };
  static const uint8_t GLYPH_C[7]     = { 0x0Eu, 0x11u, 0x10u, 0x10u, 0x10u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_D[7]     = { 0x1Cu, 0x12u, 0x11u, 0x11u, 0x11u, 0x12u, 0x1Cu };
  static const uint8_t GLYPH_E[7]     = { 0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x10u, 0x10u, 0x1Fu };
  static const uint8_t GLYPH_F[7]     = { 0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x10u, 0x10u, 0x10u };
  static const uint8_t GLYPH_G[7]     = { 0x0Eu, 0x11u, 0x10u, 0x17u, 0x11u, 0x11u, 0x0Fu };
  static const uint8_t GLYPH_H[7]     = { 0x11u, 0x11u, 0x11u, 0x1Fu, 0x11u, 0x11u, 0x11u };
  static const uint8_t GLYPH_I[7]     = { 0x1Fu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x1Fu };
  static const uint8_t GLYPH_J[7]     = { 0x07u, 0x02u, 0x02u, 0x02u, 0x12u, 0x12u, 0x0Cu };
  static const uint8_t GLYPH_K[7]     = { 0x11u, 0x12u, 0x14u, 0x18u, 0x14u, 0x12u, 0x11u };
  static const uint8_t GLYPH_L[7]     = { 0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x1Fu };
  static const uint8_t GLYPH_M[7]     = { 0x11u, 0x1Bu, 0x15u, 0x15u, 0x11u, 0x11u, 0x11u };
  static const uint8_t GLYPH_N[7]     = { 0x11u, 0x19u, 0x15u, 0x13u, 0x11u, 0x11u, 0x11u };
  static const uint8_t GLYPH_O[7]     = { 0x0Eu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_P[7]     = { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x10u, 0x10u, 0x10u };
  static const uint8_t GLYPH_Q[7]     = { 0x0Eu, 0x11u, 0x11u, 0x11u, 0x15u, 0x12u, 0x0Du };
  static const uint8_t GLYPH_R[7]     = { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x14u, 0x12u, 0x11u };
  static const uint8_t GLYPH_S[7]     = { 0x0Fu, 0x10u, 0x10u, 0x0Eu, 0x01u, 0x01u, 0x1Eu };
  static const uint8_t GLYPH_T[7]     = { 0x1Fu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u };
  static const uint8_t GLYPH_U[7]     = { 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_V[7]     = { 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Au, 0x04u };
  static const uint8_t GLYPH_W[7]     = { 0x11u, 0x11u, 0x11u, 0x15u, 0x15u, 0x15u, 0x0Au };
  static const uint8_t GLYPH_X[7]     = { 0x11u, 0x11u, 0x0Au, 0x04u, 0x0Au, 0x11u, 0x11u };
  static const uint8_t GLYPH_Y[7]     = { 0x11u, 0x11u, 0x0Au, 0x04u, 0x04u, 0x04u, 0x04u };
  static const uint8_t GLYPH_Z[7]     = { 0x1Fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x1Fu };

  switch ((uint8_t)toupper((int)ch))
  {
    case '0': return GLYPH_0;
    case '1': return GLYPH_1;
    case '2': return GLYPH_2;
    case '3': return GLYPH_3;
    case '4': return GLYPH_4;
    case '5': return GLYPH_5;
    case '6': return GLYPH_6;
    case '7': return GLYPH_7;
    case '8': return GLYPH_8;
    case '9': return GLYPH_9;
    case 'A': return GLYPH_A;
    case 'B': return GLYPH_B;
    case 'C': return GLYPH_C;
    case 'D': return GLYPH_D;
    case 'E': return GLYPH_E;
    case 'F': return GLYPH_F;
    case 'G': return GLYPH_G;
    case 'H': return GLYPH_H;
    case 'I': return GLYPH_I;
    case 'J': return GLYPH_J;
    case 'K': return GLYPH_K;
    case 'L': return GLYPH_L;
    case 'M': return GLYPH_M;
    case 'N': return GLYPH_N;
    case 'O': return GLYPH_O;
    case 'P': return GLYPH_P;
    case 'Q': return GLYPH_Q;
    case 'R': return GLYPH_R;
    case 'S': return GLYPH_S;
    case 'T': return GLYPH_T;
    case 'U': return GLYPH_U;
    case 'V': return GLYPH_V;
    case 'W': return GLYPH_W;
    case 'X': return GLYPH_X;
    case 'Y': return GLYPH_Y;
    case 'Z': return GLYPH_Z;
    case '.': return GLYPH_DOT;
    case ':': return GLYPH_COLON;
    case '-': return GLYPH_DASH;
    case '!': return GLYPH_EXCL;
    case ' ': return GLYPH_SPACE;
    default:  return GLYPH_SPACE;
  }
}

static uint8_t decode_debug_text_char(const VMGPContext *ctx, uint32_t str_addr, uint32_t char_index, uint8_t ch)
{
  (void)ctx;
  (void)str_addr;
  (void)char_index;

  switch (ch)
  {
    case 0x01u:
      return (uint8_t)'N';

    default:
      return ch;
  }
}

/**
 * @brief Draws one byte string with a tiny host-side 5x7 debug font fallback.
 */
static int draw_debug_text_bytes(SDL_Renderer *renderer,
                                 const VMGPContext *ctx,
                                 const uint8_t *text,
                                 uint32_t char_count,
                                 const uint32_t *palette,
                                 uint32_t str_addr,
                                 int32_t x,
                                 int32_t y)
{
  const int32_t char_advance = 6;
  uint32_t char_index;
  uint32_t row;
  uint32_t col;
  const uint8_t *glyph;
  uint8_t ch;
  uint8_t fg_red;
  uint8_t fg_green;
  uint8_t fg_blue;
  uint8_t shadow_red;
  uint8_t shadow_green;
  uint8_t shadow_blue;

  if (!renderer || !ctx || !text)
  {
    return 0;
  }

  decode_guest_color(palette ? palette[1] : 0xFFu, &fg_red, &fg_green, &fg_blue);
  decode_guest_color(palette ? palette[2] : 0x00u, &shadow_red, &shadow_green, &shadow_blue);

  for (char_index = 0u; char_index < char_count; ++char_index)
  {
    uint8_t raw_ch;

    raw_ch = text[char_index];
    ch = decode_debug_text_char(ctx, str_addr, char_index, raw_ch);

    if (raw_ch == 0x01u)
    {
      const int32_t glyph_x = x + (int32_t)(char_index * (uint32_t)char_advance);

      SDL_SetRenderDrawColor(renderer, shadow_red, shadow_green, shadow_blue, 255u);
      SDL_RenderDrawLine(renderer, glyph_x + 1, y + 1, glyph_x + 1, y + 7);
      SDL_RenderDrawLine(renderer, glyph_x + 5, y + 1, glyph_x + 5, y + 7);
      SDL_RenderDrawLine(renderer, glyph_x + 1, y + 1, glyph_x + 5, y + 7);
      SDL_SetRenderDrawColor(renderer, fg_red, fg_green, fg_blue, 255u);
      SDL_RenderDrawLine(renderer, glyph_x + 0, y + 0, glyph_x + 0, y + 6);
      SDL_RenderDrawLine(renderer, glyph_x + 4, y + 0, glyph_x + 4, y + 6);
      SDL_RenderDrawLine(renderer, glyph_x + 0, y + 0, glyph_x + 4, y + 6);
      continue;
    }

    glyph = debug_font_glyph(ch);

    for (row = 0u; row < 7u; ++row)
    {
      for (col = 0u; col < 5u; ++col)
      {
        if ((glyph[row] & (1u << (4u - col))) == 0u)
        {
          continue;
        }

        SDL_SetRenderDrawColor(renderer, shadow_red, shadow_green, shadow_blue, 255u);
        SDL_RenderDrawPoint(renderer,
                            x + (int32_t)(char_index * (uint32_t)char_advance) + (int32_t)col + 1,
                            y + (int32_t)row + 1);
        SDL_SetRenderDrawColor(renderer, fg_red, fg_green, fg_blue, 255u);
        SDL_RenderDrawPoint(renderer,
                            x + (int32_t)(char_index * (uint32_t)char_advance) + (int32_t)col,
                            y + (int32_t)row);
      }
    }
  }

  return 1;
}

/**
 * @brief Draws one guest text string using the active VM font data.
 */
static int draw_guest_text(SDL_Renderer *renderer, const VMGPContext *ctx, const MVM_DrawCommand_t *command)
{
  VmFontHeader font;
  uint32_t str_addr;
  uint32_t glyph_data_addr;
  uint32_t pixel_count;
  uint32_t bits_per_char;
  uint32_t bytes_per_char_tight;
  uint32_t char_count;
  uint32_t char_index;
  int32_t base_x;
  int32_t base_y;
  int32_t draw_x;
  int32_t draw_y;
  uint8_t ch;
  uint8_t glyph_index;
  uint32_t bit_index;
  uint32_t pixel_value;
  uint32_t max_pixel_value;
  uint32_t bytes_per_char;
  const uint8_t *glyph_data;
  uint32_t drawn_pixels;
  uint8_t fg_red;
  uint8_t fg_green;
  uint8_t fg_blue;
  uint8_t bg_red;
  uint8_t bg_green;
  uint8_t bg_blue;
  const uint8_t *text_bytes;

  if (!renderer || !ctx || !command || command->aux2 == 0u)
  {
    return 0;
  }

  str_addr = command->aux;
  text_bytes = NULL;
  char_count = 0u;
  if (command->text_length != 0u)
  {
    text_bytes = command->text;
    char_count = command->text_length;
  }
  else if (str_addr < ctx->mem_size)
  {
    text_bytes = ctx->mem + str_addr;
    char_count = MVM_RuntimeStrLen(ctx->mem + str_addr, ctx->mem_size - str_addr);
  }

  if (!read_guest_font_header(ctx, command->aux2, &font))
  {
    return draw_debug_text_bytes(renderer,
                                 ctx,
                                 text_bytes,
                                 char_count,
                                 command->text_palette,
                                 str_addr,
                                 command->x0,
                                 command->y0);
  }

  if ((font.bpp != 1u && font.bpp != 2u) || font.width == 0u || font.height == 0u)
  {
    return draw_debug_text_bytes(renderer,
                                 ctx,
                                 text_bytes,
                                 char_count,
                                 command->text_palette,
                                 str_addr,
                                 command->x0,
                                 command->y0);
  }

  pixel_count = (uint32_t)font.width * (uint32_t)font.height;
  bits_per_char = pixel_count * (uint32_t)font.bpp;
  bytes_per_char_tight = (bits_per_char + 7u) / 8u;
  if (bytes_per_char_tight == 0u)
  {
    return 0;
  }

  if (!text_bytes)
  {
    return 0;
  }

  base_x = command->x0;
  base_y = command->y0;
  drawn_pixels = 0u;
  max_pixel_value = (1u << font.bpp) - 1u;
  decode_guest_color(command->text_palette[1], &fg_red, &fg_green, &fg_blue);
  decode_guest_color(command->text_palette[0], &bg_red, &bg_green, &bg_blue);

  for (char_index = 0u; char_index < char_count; ++char_index)
  {
    ch = text_bytes[char_index];
    if (!MVM_RuntimeMemRangeOk(ctx, font.char_table_addr + ch, 1u))
    {
      continue;
    }

    glyph_index = ctx->mem[font.char_table_addr + ch];
    if (glyph_index == 0xFFu)
    {
      continue;
    }

    bytes_per_char = bytes_per_char_tight;
    glyph_data_addr = font.font_data_addr + ((uint32_t)glyph_index * bytes_per_char);
    if (!MVM_RuntimeMemRangeOk(ctx, glyph_data_addr, bytes_per_char))
    {
      continue;
    }

    glyph_data = ctx->mem + glyph_data_addr;

    for (bit_index = 0u; bit_index < pixel_count; ++bit_index)
    {
      uint32_t pixel_x;
      uint32_t pixel_y;
      uint8_t red;
      uint8_t green;
      uint8_t blue;

      pixel_value = read_tight_font_pixel(glyph_data, bit_index, (uint32_t)font.bpp, 0);
      if (pixel_value == 0u)
      {
        continue;
      }

      pixel_x = bit_index % (uint32_t)font.width;
      pixel_y = bit_index / (uint32_t)font.width;
      draw_x = base_x + (int32_t)(char_index * font.width) + (int32_t)pixel_x;
      draw_y = base_y + (int32_t)pixel_y;
      if (font.bpp > 1u && (uint32_t)font.palindex + pixel_value < 4u)
      {
        decode_guest_color(command->text_palette[(uint32_t)font.palindex + pixel_value], &red, &green, &blue);
      }
      else if (font.bpp == 1u)
      {
        decode_guest_color(command->color, &red, &green, &blue);
      }
      else
      {
        red = (uint8_t)((((max_pixel_value - pixel_value) * bg_red) + (pixel_value * fg_red)) / max_pixel_value);
        green = (uint8_t)((((max_pixel_value - pixel_value) * bg_green) + (pixel_value * fg_green)) / max_pixel_value);
        blue = (uint8_t)((((max_pixel_value - pixel_value) * bg_blue) + (pixel_value * fg_blue)) / max_pixel_value);
      }
      SDL_SetRenderDrawColor(renderer, red, green, blue, 255u);
      SDL_RenderDrawPoint(renderer, draw_x, draw_y);
      ++drawn_pixels;
    }
  }

  return drawn_pixels != 0u ? 1 : draw_debug_text_bytes(renderer,
                                                        ctx,
                                                        text_bytes,
                                                        char_count,
                                                        command->text_palette,
                                                        str_addr,
                                                        command->x0,
                                                        command->y0);
}

/**
 * @brief Tries to draw one 8x8 tile from one SDK MAP_HEADER tile-data area.
 */
static int draw_guest_map_tile(SDL_Renderer *renderer,
                               const VMGPContext *ctx,
                               const VMGPMapState *map_state,
                               uint8_t tile_index,
                               uint8_t tile_attribute,
                               int32_t x,
                               int32_t y)
{
  uint32_t format;
  uint32_t bits_per_pixel;
  uint32_t byte_count;
  uint32_t data_addr;
  uint32_t pixel_index;
  uint32_t index;
  uint32_t source_x;
  uint32_t source_y;
  uint8_t pixel;
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  if (!renderer || !ctx || !map_state || !map_state->valid || map_state->tile_data_addr == 0u || tile_index == 0u)
  {
    return 0;
  }

  format = (uint32_t)(map_state->format & 0x07u);
  bits_per_pixel = sprite_format_bits_per_pixel((uint8_t)format);
  if (bits_per_pixel == 0u)
  {
    return 0;
  }

  byte_count = (64u * bits_per_pixel + 7u) / 8u;
  if ((tile_attribute & 0x40u) != 0u)
  {
    tile_index = (uint8_t)(tile_index + (map_state->animation_active & 1u));
  }
  else if ((tile_attribute & 0x80u) != 0u)
  {
    tile_index = (uint8_t)(tile_index + (map_state->animation_active & 3u));
  }

  data_addr = map_state->tile_data_addr + (((uint32_t)tile_index - 1u) * byte_count);
  if (!MVM_RuntimeMemRangeOk(ctx, data_addr, byte_count))
  {
    return 0;
  }

  for (index = 0u; index < 64u; ++index)
  {
    source_x = index & 7u;
    source_y = index >> 3;
    if ((tile_attribute & 0x02u) != 0u)
    {
      source_x = 7u - source_x;
    }

    if ((tile_attribute & 0x04u) != 0u)
    {
      source_y = 7u - source_y;
    }

    pixel_index = read_packed_sprite_pixel(ctx->mem + data_addr, (source_y * 8u) + source_x, bits_per_pixel);
    if (pixel_index == 0u && (map_state->flags & 0x01u) != 0u && (tile_attribute & 0x01u) != 0u)
    {
      continue;
    }

    switch (format)
    {
      case 0x00u:
        red = pixel_index ? 255u : 0u;
        green = red;
        blue = red;
        break;

      case 0x01u:
        red = (uint8_t)(pixel_index * 85u);
        green = red;
        blue = red;
        break;

      case 0x02u:
        red = (uint8_t)(pixel_index * 17u);
        green = red;
        blue = red;
        break;

      case 0x03u:
      case 0x04u:
      case 0x05u:
      case 0x06u:
        pixel = (uint8_t)(tile_attribute + pixel_index);
        decode_guest_color(ctx->palette_entries[pixel], &red, &green, &blue);
        break;

      case 0x07u:
        pixel = (uint8_t)pixel_index;
        red = (uint8_t)(((pixel >> 5) & 0x07u) * 255u / 7u);
        green = (uint8_t)(((pixel >> 2) & 0x07u) * 255u / 7u);
        blue = (uint8_t)((pixel & 0x03u) * 255u / 3u);
        break;

      default:
        return 0;
    }

    SDL_SetRenderDrawColor(renderer, red, green, blue, 255u);
    SDL_RenderDrawPoint(renderer, x + (int32_t)(index & 7u), y + (int32_t)(index >> 3));
  }

  return 1;
}

/**
 * @brief Draws one captured VM tilemap command.
 */
static void render_guest_map(SDL_Renderer *renderer, const VMGPContext *ctx, const MVM_DrawCommand_t *command)
{
  const VMGPMapState *map_state;
  const uint8_t *snapshot;
  uint32_t stride;
  uint32_t map_x;
  uint32_t map_y;
  uint32_t screen_x;
  uint32_t screen_y;
  uint32_t offset;
  int32_t source_x;
  int32_t source_y;
  int32_t dest_origin_x;
  int32_t dest_origin_y;
  int32_t pixel_offset_x;
  int32_t pixel_offset_y;
  uint8_t tile_index;
  uint8_t tile_attribute;

  if (!renderer || !ctx || !command)
  {
    return;
  }

  map_state = &command->map_state;
  if (!map_state->valid || map_state->width == 0u || map_state->height == 0u)
  {
    return;
  }

  stride = (map_state->flags != 0u) ? 2u : 1u;
  snapshot = NULL;
  if (command->map_snapshot_length >= ((uint32_t)map_state->width * (uint32_t)map_state->height * stride) &&
      command->map_snapshot_offset <= VMGP_DRAW_MAP_SNAPSHOT_POOL_BYTES &&
      command->map_snapshot_length <= (VMGP_DRAW_MAP_SNAPSHOT_POOL_BYTES - command->map_snapshot_offset))
  {
    snapshot = ctx->map_snapshot_pool + command->map_snapshot_offset;
  }

  source_x = (int32_t)map_state->x_pos;
  source_y = (int32_t)map_state->y_pos;
  dest_origin_x = (int32_t)map_state->x_pan;
  dest_origin_y = (int32_t)map_state->y_pan;

  if (source_x < 0)
  {
    dest_origin_x -= source_x;
    source_x = 0;
  }

  if (source_y < 0)
  {
    dest_origin_y -= source_y;
    source_y = 0;
  }

  map_x = (uint32_t)source_x >> 3;
  map_y = (uint32_t)source_y >> 3;
  pixel_offset_x = source_x & 7;
  pixel_offset_y = source_y & 7;

  for (screen_y = 0u; (map_y + screen_y) < map_state->height; ++screen_y)
  {
    for (screen_x = 0u; (map_x + screen_x) < map_state->width; ++screen_x)
    {
      offset = map_state->map_data_addr +
               (((map_y + screen_y) * (uint32_t)map_state->width) + (map_x + screen_x)) * stride;
      if (snapshot)
      {
        offset = (((map_y + screen_y) * (uint32_t)map_state->width) + (map_x + screen_x)) * stride;
        tile_index = snapshot[offset];
        tile_attribute = (stride > 1u) ? snapshot[offset + 1u] : 0u;
      }
      else
      {
        if (!MVM_RuntimeMemRangeOk(ctx, offset, stride))
        {
          continue;
        }

        tile_index = ctx->mem[offset];
        tile_attribute = (stride > 1u) ? ctx->mem[offset + 1u] : 0u;
      }

      draw_guest_map_tile(renderer,
                          ctx,
                          map_state,
                          tile_index,
                          tile_attribute,
                          dest_origin_x + (int32_t)(screen_x * 8u) - pixel_offset_x,
                          dest_origin_y + (int32_t)(screen_y * 8u) - pixel_offset_y);
    }
  }
}

/**
 * @brief Draws the active VM sprite-slot table as persistent scene state.
 */
static void render_guest_sprite_slots(SDL_Renderer *renderer, const VMGPContext *ctx)
{
  MVM_DrawCommand_t command;
  VmSpriteHeader sprite;
  uint32_t index;

  if (!renderer || !ctx)
  {
    return;
  }

  for (index = 0u; index < ctx->sprite_slot_count; ++index)
  {
    if (!ctx->sprite_slots[index].used)
    {
      continue;
    }

    memset(&command, 0, sizeof(command));
    command.type = MVM_DRAW_SPRITE;
    command.x0 = ctx->sprite_slots[index].x;
    command.y0 = ctx->sprite_slots[index].y;
    command.aux = ctx->sprite_slots[index].sprite_addr;
    if (read_guest_sprite_header(ctx, command.aux, &sprite))
    {
      command.width = sprite.width;
      command.height = sprite.height;
    }

    (void)draw_guest_sprite(renderer, ctx, &command);
  }
}

/**
 * @brief Restores the clip window captured with one deferred draw command.
 */
static void apply_command_clip(SDL_Renderer *renderer, const MVM_DrawCommand_t *command)
{
  SDL_Rect clip_rect;

  if (!renderer || !command)
  {
    return;
  }

  if (command->clip_x1 > command->clip_x0 && command->clip_y1 > command->clip_y0)
  {
    clip_rect.x = (int)command->clip_x0;
    clip_rect.y = (int)command->clip_y0;
    clip_rect.w = (int)(command->clip_x1 - command->clip_x0);
    clip_rect.h = (int)(command->clip_y1 - command->clip_y0);
    SDL_RenderSetClipRect(renderer, &clip_rect);
  }
  else
  {
    SDL_RenderSetClipRect(renderer, NULL);
  }
}

/**
 * @brief Opens one image file for source-backed access.
 */
static int open_image_source(const char *path, FileImageSource *provider)
{
  long size;

  provider->file = fopen(path, "rb");
  if (!provider->file)
  {
    fprintf(stderr, "Failed to open: %s\n", path);

    return 0;
  }

  if (fseek(provider->file, 0, SEEK_END) != 0)
  {
    fclose(provider->file);
    provider->file = NULL;

    return 0;
  }

  size = ftell(provider->file);
  if (size < 0)
  {
    fclose(provider->file);
    provider->file = NULL;

    return 0;
  }

  rewind(provider->file);
  provider->size = (size_t)size;

  return 1;
}

/**
 * @brief Closes one file-backed image source.
 */
static void close_image_source(FileImageSource *provider)
{
  if (provider && provider->file)
  {
    fclose(provider->file);
    provider->file = NULL;
    provider->size = 0u;
  }
}

/**
 * @brief Prints the command-line usage string.
 */
static void print_usage(const char *program_name)
{
  fprintf(stderr,
          "Usage: %s <decrypted.mpn> [profile_name] [max_steps] [max_logged_calls]\n",
          program_name);
}

/**
 * @brief Prints the names of all configured device profiles.
 */
static void print_available_profiles(void)
{
  uint32_t i;
  uint32_t profile_count;
  const MpnDevProfile_t *profile;

  fprintf(stderr, "Available profiles:");

  profile_count = MVM_GetDevProfileCount();
  for (i = 0u; i < profile_count; ++i)
  {
    profile = MVM_GetDevProfile(i);
    if (profile && profile->name)
    {
      fprintf(stderr, " %s", profile->name);
    }
  }

  fprintf(stderr, "\n");
}

/**
 * @brief Initializes the minimal SDL window/renderer backend for one profile.
 */
static int init_sdl_backend(const MpnDevProfile_t *profile, SdlBackend *backend)
{
  uint32_t width;
  uint32_t height;

  if (!backend)
  {
    return 0;
  }

  backend->window = NULL;
  backend->renderer = NULL;
  backend->width = 0u;
  backend->height = 0u;
  backend->raw_button_state = 0u;
  backend->pulse_button_state = 0u;
  memset(backend->next_repeat_ms, 0, sizeof(backend->next_repeat_ms));
  backend->last_frame_serial = 0u;

  width = 320u;
  height = 240u;
  if (profile)
  {
    if (profile->screen_width != 0u)
    {
      width = profile->screen_width;
    }

    if (profile->screen_height != 0u)
    {
      height = profile->screen_height;
    }
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0)
  {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());

    return 0;
  }

  backend->window = SDL_CreateWindow("Mophun VM",
                                     SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED,
                                     (int)(width * 2u),
                                     (int)(height * 2u),
                                     SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!backend->window)
  {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();

    return 0;
  }

  backend->renderer = SDL_CreateRenderer(backend->window,
                                         -1,
                                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!backend->renderer)
  {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(backend->window);
    backend->window = NULL;
    SDL_Quit();

    return 0;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  SDL_RenderSetLogicalSize(backend->renderer, (int)width, (int)height);

  backend->width = width;
  backend->height = height;

  return 1;
}

/**
 * @brief Updates one raw host button bit and emits one edge-triggered pulse.
 */
static void update_backend_raw_button_mask(SdlBackend *backend, uint32_t mask, int is_down);

/**
 * @brief Converts one SDL keyboard event into one VM button-mask update.
 */
/*
 * SDK emulator mapping:
 * - arrows     -> directions
 * - Shift/Ctrl -> Fire
 * - Backspace  -> Select
 * - Enter      -> Select
 * - Space      -> Fire2
 * - Shift      -> pointer modifier ignored by the generic button path
 */
static void update_backend_button_state(SdlBackend *backend, const SDL_KeyboardEvent *key_event)
{
  uint32_t mask;

  if (!backend || !key_event)
  {
    return;
  }

  mask = 0u;
  switch (key_event->keysym.sym)
  {
    case SDLK_UP:
    case SDLK_KP_8:
      mask = MVM_KEY_UP;
      break;

    case SDLK_DOWN:
    case SDLK_KP_2:
      mask = MVM_KEY_DOWN;
      break;

    case SDLK_LEFT:
    case SDLK_KP_4:
      mask = MVM_KEY_LEFT;
      break;

    case SDLK_RIGHT:
    case SDLK_KP_6:
      mask = MVM_KEY_RIGHT;
      break;

    case SDLK_KP_7:
      mask = MVM_KEY_UP | MVM_KEY_LEFT;
      break;

    case SDLK_KP_9:
      mask = MVM_KEY_UP | MVM_KEY_RIGHT;
      break;

    case SDLK_KP_1:
      mask = MVM_KEY_DOWN | MVM_KEY_LEFT;
      break;

    case SDLK_KP_3:
      mask = MVM_KEY_DOWN | MVM_KEY_RIGHT;
      break;

    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
      mask = MVM_KEY_FIRE;
      break;

    case SDLK_BACKSPACE:
    case SDLK_RETURN:
      mask = MVM_KEY_SELECT;
      break;

    case SDLK_SPACE:
    case SDLK_KP_ENTER:
      mask = MVM_KEY_FIRE2;
      break;

    default:
      break;
  }

  if (mask == 0u)
  {
    return;
  }

  update_backend_raw_button_mask(backend, mask, key_event->type == SDL_KEYDOWN);
}

/**
 * @brief Converts one SDL mouse-button event into one VM button-mask update.
 */
static void update_backend_pointer_state(SdlBackend *backend, const SDL_MouseButtonEvent *button_event)
{
  if (!backend || !button_event)
  {
    return;
  }

  /*
   * Pointer button bits are intentionally ignored for now.
   *
   * The current desktop backend does not yet implement the full pointer path
   * (`vGetPointerPos` plus pointer-driven UI semantics). Feeding
   * POINTER_DOWN/POINTER_ALTDOWN into `vGetButtonData()` makes the sample game
   * immediately take its exit path, so mouse clicks stay out of the generic
   * button mask until the pointer API is implemented coherently.
   */
  (void)backend;
  (void)button_event;
}

/**
 * @brief Returns one monotonic host tick value in milliseconds via SDL.
 */
static uint32_t sdl_platform_get_ticks_ms(void *user)
{
  (void)user;

  return (uint32_t)SDL_GetTicks();
}

/**
 * @brief Updates one raw host button bit and emits one edge-triggered pulse.
 */
static void update_backend_raw_button_mask(SdlBackend *backend, uint32_t mask, int is_down)
{
  uint32_t now_ms;
  uint32_t i;

  if (!backend || mask == 0u)
  {
    return;
  }

  now_ms = sdl_platform_get_ticks_ms(backend);
  for (i = 0u; i < SDL_BACKEND_BUTTON_COUNT; ++i)
  {
    if ((mask & MVM_lSdlButtonMasks[i]) == 0u)
    {
      continue;
    }

    if (is_down)
    {
      if ((backend->raw_button_state & MVM_lSdlButtonMasks[i]) == 0u)
      {
        backend->raw_button_state |= MVM_lSdlButtonMasks[i];
        backend->pulse_button_state |= MVM_lSdlButtonMasks[i];
        backend->next_repeat_ms[i] = now_ms + INPUT_REPEAT_DELAY_MS;
      }
    }
    else
    {
      backend->raw_button_state &= ~MVM_lSdlButtonMasks[i];
      backend->next_repeat_ms[i] = 0u;
    }
  }
}

/**
 * @brief Generates held-key repeat pulses with a phone-style repeat delay.
 */
static void refresh_backend_button_pulses(SdlBackend *backend)
{
  uint32_t now_ms;
  uint32_t i;

  if (!backend)
  {
    return;
  }

  now_ms = sdl_platform_get_ticks_ms(backend);
  for (i = 0u; i < SDL_BACKEND_BUTTON_COUNT; ++i)
  {
    if ((backend->raw_button_state & MVM_lSdlButtonMasks[i]) == 0u)
    {
      continue;
    }

    if (backend->next_repeat_ms[i] == 0u)
    {
      continue;
    }

    if (now_ms < backend->next_repeat_ms[i])
    {
      continue;
    }

    backend->pulse_button_state |= MVM_lSdlButtonMasks[i];
    backend->next_repeat_ms[i] = now_ms + INPUT_REPEAT_INTERVAL_MS;
  }
}

/**
 * @brief Attaches SDL-backed timing callbacks to the current VM instance.
 */
static void attach_backend_platform_timing(MpnVM_t *vm, SdlBackend *backend)
{
  VMGPContext *ctx;

  if (!vm)
  {
    return;
  }

  ctx = (VMGPContext *)vm;
  ctx->platform.user = backend;
  ctx->platform.get_ticks_ms = sdl_platform_get_ticks_ms;
  ctx->tick_count = sdl_platform_get_ticks_ms(backend);
}

/**
 * @brief Copies the host button mask into the current VM instance.
 */
static void sync_backend_input_to_vm(MpnVM_t *vm, SdlBackend *backend)
{
  VMGPContext *ctx;

  if (!vm || !backend)
  {
    return;
  }

  ctx = (VMGPContext *)vm;
  ctx->button_state = backend->raw_button_state | backend->pulse_button_state;
  backend->pulse_button_state = 0u;
}

/**
 * @brief Shuts down the minimal SDL backend.
 */
static void shutdown_sdl_backend(SdlBackend *backend)
{
  if (!backend)
  {
    return;
  }

  if (backend->renderer)
  {
    SDL_DestroyRenderer(backend->renderer);
    backend->renderer = NULL;
  }

  if (backend->window)
  {
    SDL_DestroyWindow(backend->window);
    backend->window = NULL;
  }

  backend->width = 0u;
  backend->height = 0u;
  SDL_Quit();
}

/**
 * @brief Presents the minimal host window for one frame.
 */
static void present_sdl_backend(MpnVM_t *vm, SdlBackend *backend)
{
  VMGPContext *ctx;
  const MVM_DrawCommand_t *command;
  SDL_Rect rect;
  uint32_t index;
  uint32_t color;

  if (!backend || !backend->renderer)
  {
    return;
  }

  if (!vm)
  {
    return;
  }

  color = 0u;
  ctx = (VMGPContext *)vm;
  color = ctx->clear_color;

  set_renderer_guest_color(backend->renderer, color);
  SDL_RenderClear(backend->renderer);

  for (index = 0u; index < ctx->draw_command_count; ++index)
  {
    command = &ctx->draw_commands[index];

    if (command->type == MVM_DRAW_TEXT)
    {
      continue;
    }

    apply_command_clip(backend->renderer, command);
    set_renderer_guest_color(backend->renderer, command->color);

    switch (command->type)
    {
      case MVM_DRAW_FILL_RECT:
        rect.x = command->x0;
        rect.y = command->y0;
        rect.w = command->x1 - command->x0;
        rect.h = command->y1 - command->y0;
        if (rect.w < 0)
        {
          rect.x += rect.w;
          rect.w = -rect.w;
        }
        if (rect.h < 0)
        {
          rect.y += rect.h;
          rect.h = -rect.h;
        }
        SDL_RenderFillRect(backend->renderer, &rect);
        break;

      case MVM_DRAW_LINE:
        SDL_RenderDrawLine(backend->renderer,
                           command->x0,
                           command->y0,
                           command->x1,
                           command->y1);
        break;

      case MVM_DRAW_SPRITE:
        if (!draw_guest_sprite(backend->renderer, ctx, command))
        {
          SDL_SetRenderDrawColor(backend->renderer, 255u, 255u, 255u, 255u);
          rect.x = command->x0;
          rect.y = command->y0;
          rect.w = command->width != 0u ? command->width : 8;
          rect.h = command->height != 0u ? command->height : 8;
          SDL_RenderDrawRect(backend->renderer, &rect);
        }
        break;

      case MVM_DRAW_MAP:
        render_guest_map(backend->renderer, ctx, command);
        break;

      case MVM_DRAW_SPRITE_SLOTS:
        render_guest_sprite_slots(backend->renderer, ctx);
        break;

      default:
        break;
    }
  }

  for (index = 0u; index < ctx->draw_command_count; ++index)
  {
    command = &ctx->draw_commands[index];
    if (command->type != MVM_DRAW_TEXT)
    {
      continue;
    }

    set_renderer_guest_color(backend->renderer, command->color);
    apply_command_clip(backend->renderer, command);
    if (!draw_guest_text(backend->renderer, ctx, command))
    {
      SDL_SetRenderDrawColor(backend->renderer, 255u, 255u, 0u, 255u);
      rect.x = command->x0;
      rect.y = command->y0;
      rect.w = command->width != 0u ? command->width : 8;
      rect.h = command->height != 0u ? command->height : 8;
      SDL_RenderDrawRect(backend->renderer, &rect);
    }
  }

  SDL_RenderSetClipRect(backend->renderer, NULL);

  if (ctx->frame_serial != backend->last_frame_serial)
  {
    backend->last_frame_serial = ctx->frame_serial;
  }

  SDL_RenderPresent(backend->renderer);
  ctx->draw_command_count = 0u;
  ctx->map_snapshot_pool_used = 0u;
}

/**
 * @brief Executes one bounded VM step.
 */
static int pump_vm_once(MpnVM_t *vm, uint32_t *step_budget)
{
  MVM_RetCode_t retVal;
  MVM_State_t state;

  state = MVM_GetState(vm);
  if (state != MVM_STATE_READY && state != MVM_STATE_RUNNING)
  {
    return 1;
  }

  if (*step_budget == 0u)
  {
    return 1;
  }

  retVal = MVM_RunStep(vm);
  if (MVM_OK != retVal)
  {
    return 1;
  }

  --(*step_budget);

  return 0;
}

/**
 * @brief Checks whether one argument contains only decimal digits.
 */
static int is_numeric_arg(const char *value)
{
  const unsigned char *p;

  if (!value || !*value)
  {
    return 0;
  }

  p = (const unsigned char *)value;
  while (*p != '\0')
  {
    if (!isdigit(*p))
    {
      return 0;
    }
    ++p;
  }

  return 1;
}

/**
 * @brief Parses runner options from the command line.
 */
static int parse_options(int argc, char **argv, AppOptions *options)
{
  int arg_index;

  if (argc < 2 || argc > 5)
  {
    print_usage(argv[0]);

    return 0;
  }

  options->image_path = argv[1];
  options->profile_name = NULL;
  options->max_steps = MAX_STEPS_DEFAULT;
  options->max_logged_calls = MAX_LOGGED_CALLS_DEFAULT;

  arg_index = 2;
  if (argc > arg_index && !is_numeric_arg(argv[arg_index]))
  {
    options->profile_name = argv[arg_index];
    ++arg_index;
  }

  if (argc > arg_index)
  {
    options->max_steps = (uint32_t)strtoul(argv[arg_index], NULL, 0);
    ++arg_index;
  }

  if (argc > arg_index)
  {
    options->max_logged_calls = (uint32_t)strtoul(argv[arg_index], NULL, 0);
  }

  return 1;
}

/**
 * @brief Selects the active device profile for the current run.
 */
static int validate_device_profile(const char *profile_name)
{
  if (!profile_name)
  {
    return 1;
  }

  if (!MVM_FindDevProfileByName(profile_name))
  {
    fprintf(stderr, "Unknown device profile: %s\n", profile_name);
    print_available_profiles();

    return 0;
  }

  return 1;
}

/**
 * @brief Creates a VM view over caller-provided storage.
 */
static MpnVM_t *create_vm(void *storage)
{
  size_t storage_size;
  MpnVM_t *vm;

  storage_size = MVM_GetStorageSize();
  vm = MVM_GetVmFromStorage(storage, storage_size);

  return vm;
}

/**
 * @brief Runs the VM until one local stop condition is reached.
 */
static int run_vm(MpnVM_t *vm, SdlBackend *backend, uint32_t max_steps, uint32_t max_logged_calls)
{
  VMGPContext *ctx;
  MVM_RetCode_t retVal;
  SDL_Event event;
  uint32_t host_budget;
  uint32_t frame_serial_before;
  uint32_t step_budget;
  int quit_requested;

  MVM_DumpVmgpSummary(vm);
  MVM_DumpVmgpImports(vm, 64);
  retVal = MVM_SetWdgLimit(vm, 0u);
  if (MVM_OK != retVal)
  {
    return 0;
  }

  attach_backend_platform_timing(vm, backend);
  present_sdl_backend(vm, backend);

  ctx = (VMGPContext *)vm;
  step_budget = max_steps;
  quit_requested = 0;
  while (!quit_requested)
  {
    while (SDL_PollEvent(&event) != 0)
    {
      if (event.type == SDL_QUIT)
      {
        MVM_RequestExit(vm);
        quit_requested = 1;
        break;
      }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
        {
          update_backend_button_state(backend, &event.key);
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
        {
          update_backend_pointer_state(backend, &event.button);
        }
      }

    refresh_backend_button_pulses(backend);
    sync_backend_input_to_vm(vm, backend);

    frame_serial_before = ctx->frame_serial;
    host_budget = VM_MAX_STEPS_PER_HOST_FRAME;
    while (host_budget != 0u && pump_vm_once(vm, &step_budget) == 0)
    {
      --host_budget;

      if (ctx->frame_serial != frame_serial_before)
      {
        break;
      }
    }

    present_sdl_backend(vm, backend);

    if (MVM_GetLoggedCalls(vm) >= max_logged_calls)
    {
      break;
    }

    if (MVM_GetState(vm) != MVM_STATE_READY && MVM_GetState(vm) != MVM_STATE_RUNNING)
    {
      break;
    }

    if (step_budget == 0u)
    {
      break;
    }

    SDL_Delay(HOST_MIN_LOOP_DELAY_MS);
  }

  return 1;
}

/**
 * @brief Prints the final VM execution summary.
 */
static void print_stop_summary(MpnVM_t *vm)
{
  MVM_State_t state;
  MVM_Err_t error;

  state = MVM_GetState(vm);
  error = MVM_GetLastError(vm);
  fprintf(stdout,
          "=== stop ===\nsteps=%u pc=0x%08X logged_calls=%u state=%u error=%u\n",
          MVM_GetExecutedSteps(vm),
          MVM_GetProgramCounter(vm),
          MVM_GetLoggedCalls(vm),
          (unsigned)state,
          (unsigned)error);
}

int main(int argc, char **argv)
{
  AppOptions options;
  FileImageSource file_provider;
  MpnImageSource_t image_source;
  void *vm_storage;
  MpnVM_t *vm;
  SdlBackend backend;
  MVM_MemReqs_t memory_requirements;
  const MpnDevProfile_t *profile;
  MVM_RetCode_t retVal;
  int exit_code;

  file_provider = (FileImageSource){0};
  image_source = (MpnImageSource_t){0};
  vm_storage = NULL;
  vm = NULL;
  backend = (SdlBackend){0};
  memory_requirements = (MVM_MemReqs_t){0};
  profile = NULL;
  retVal = MVM_OK;
  exit_code = 1;

  if (!parse_options(argc, argv, &options))
  {
    return exit_code;
  }

  /* Validate the requested device profile name before init so the example can
   * print a friendly list of built-in profiles.
   */
  if (!validate_device_profile(options.profile_name))
  {
    return exit_code;
  }

  profile = resolve_device_profile(options.profile_name);
  if (!profile)
  {
    fprintf(stderr, "No built-in device profile is available.\n");

    return exit_code;
  }

  if (!init_sdl_backend(profile, &backend))
  {
    return exit_code;
  }

  /* This sample integration opens the VMGP image through a file-backed image
   * source descriptor. The actual read callbacks are compiled into Config/,
   * so the runner only chooses which image instance to execute.
   */
  if (!open_image_source(options.image_path, &file_provider))
  {
    fprintf(stderr, "Could not load file.\n");
    shutdown_sdl_backend(&backend);

    return exit_code;
  }

  image_source.user = file_provider.file;
  image_source.image_size = file_provider.size;

  /* The host owns raw VM storage and asks the library to construct a VM
   * instance inside that storage block.
   */
  vm_storage = malloc(MVM_GetStorageSize());
  vm = create_vm(vm_storage);
  if (!vm)
  {
    fprintf(stderr, "Could not allocate VM storage.\n");
    close_image_source(&file_provider);
    shutdown_sdl_backend(&backend);
    free(vm_storage);

    return exit_code;
  }

  /* Query image-driven runtime memory needs before init so the integration can
   * validate its configured runtime pool capacity.
   */
  retVal = MVM_QueryMemReqsFromSource(&image_source, &memory_requirements);
  if (MVM_OK != retVal)
  {
    fprintf(stderr, "Could not query VM memory requirements. ret=%u\n", (unsigned)retVal);
    MVM_Free(vm);
    free(vm_storage);
    close_image_source(&file_provider);
    shutdown_sdl_backend(&backend);

    return exit_code;
  }

  /* Initialize the VM through the source-based public API. The host only
   * provides VM storage, the image source, and the optional device profile.
   */
  retVal = MVM_InitFromSource(vm, &image_source, options.profile_name);
  if (MVM_OK != retVal)
  {
    fprintf(stderr,
            "Failed to initialize VMGP context. ret=%u required_pool=%llu error=%u\n",
            (unsigned)retVal,
            (unsigned long long)memory_requirements.runtime_pool_bytes,
            (unsigned)MVM_GetLastError(vm));
    MVM_Free(vm);
    free(vm_storage);
    close_image_source(&file_provider);
    shutdown_sdl_backend(&backend);

    return exit_code;
  }

  /* Drive the VM through the non-blocking step API until one of the local
   * runner limits is reached.
   */
  run_vm(vm, &backend, options.max_steps, options.max_logged_calls);
  print_stop_summary(vm);

  exit_code = 0;
  MVM_Free(vm);
  free(vm_storage);
  close_image_source(&file_provider);
  shutdown_sdl_backend(&backend);

  return exit_code;
}
