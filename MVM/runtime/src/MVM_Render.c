#include "MVM_Render.h"
#include "MVM_Internal.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

typedef struct SDL_Rect
{
  int x;
  int y;
  int w;
  int h;
} SDL_Rect;

typedef struct SDL_Renderer
{
  const MVM_RenderBackend_t *backend;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} SDL_Renderer;

static void SDL_SetRenderDrawColor(SDL_Renderer *renderer, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
  (void)alpha;
  if (!renderer)
  {
    return;
  }

  renderer->red = red;
  renderer->green = green;
  renderer->blue = blue;
}

static void SDL_RenderDrawPoint(SDL_Renderer *renderer, int32_t x, int32_t y)
{
  if (renderer && renderer->backend && renderer->backend->draw_point)
  {
    renderer->backend->draw_point(renderer->backend->user, x, y, renderer->red, renderer->green, renderer->blue);
  }
}

static void SDL_RenderDrawLine(SDL_Renderer *renderer, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  if (renderer && renderer->backend && renderer->backend->draw_line)
  {
    renderer->backend->draw_line(renderer->backend->user, x0, y0, x1, y1, renderer->red, renderer->green, renderer->blue);
  }
}

static void SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect)
{
  if (renderer && renderer->backend && renderer->backend->fill_rect && rect)
  {
    renderer->backend->fill_rect(renderer->backend->user, rect->x, rect->y, rect->w, rect->h, renderer->red, renderer->green, renderer->blue);
  }
}

static void SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect)
{
  if (!renderer || !rect)
  {
    return;
  }

  SDL_RenderDrawLine(renderer, rect->x, rect->y, rect->x + rect->w - 1, rect->y);
  SDL_RenderDrawLine(renderer, rect->x, rect->y, rect->x, rect->y + rect->h - 1);
  SDL_RenderDrawLine(renderer, rect->x + rect->w - 1, rect->y, rect->x + rect->w - 1, rect->y + rect->h - 1);
  SDL_RenderDrawLine(renderer, rect->x, rect->y + rect->h - 1, rect->x + rect->w - 1, rect->y + rect->h - 1);
}

static void SDL_RenderSetClipRect(SDL_Renderer *renderer, const SDL_Rect *rect)
{
  if (!renderer || !renderer->backend || !renderer->backend->set_clip)
  {
    return;
  }

  if (rect)
  {
    renderer->backend->set_clip(renderer->backend->user, 1, rect->x, rect->y, rect->w, rect->h);
  }
  else
  {
    renderer->backend->set_clip(renderer->backend->user, 0, 0, 0, 0, 0);
  }
}

int MVM_RenderGetFrameInfo(const MpnVM_t *vm, MVM_RenderFrameInfo_t *info)
{
  const VMGPContext *ctx;

  if (!vm || !info)
  {
    return 0;
  }

  ctx = (const VMGPContext *)vm;
  info->clear_color = ctx->clear_color;
  info->clear_serial = ctx->clear_serial;
  info->draw_command_count = ctx->draw_command_count;
  info->frame_serial = ctx->frame_serial;

  return 1;
}

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
 * @brief Reads the palette captured when one deferred draw command was emitted.
 */
static uint32_t draw_command_palette_entry(const VMGPContext *ctx,
                                           const MVM_DrawCommand_t *command,
                                           uint8_t index)
{
  if (ctx &&
      command &&
      command->palette_valid != 0u &&
      command->palette_snapshot < ctx->draw_palette_count &&
      command->palette_snapshot < VMGP_MAX_DRAW_PALETTE_SNAPSHOTS)
  {
    return ctx->draw_palettes[command->palette_snapshot][index];
  }

  return ctx ? ctx->palette_entries[index] : 0u;
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
          decode_guest_color(draw_command_palette_entry(ctx, command, pixel), &red, &green, &blue);
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
        decode_guest_color(draw_command_palette_entry(ctx, command, pixel), &red, &green, &blue);
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
                               const MVM_DrawCommand_t *command,
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
  uint8_t pixel;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  int transparent_zero;

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
  data_addr = map_state->tile_data_addr + (((uint32_t)tile_index - 1u) * byte_count);
  if (!MVM_RuntimeMemRangeOk(ctx, data_addr, byte_count))
  {
    return 0;
  }

  transparent_zero = ((map_state->flags & 0x01u) != 0u) &&
                     (((map_state->flags & 0x02u) == 0u) || ((tile_attribute & 0x01u) != 0u));

  for (index = 0u; index < 64u; ++index)
  {
    uint32_t pixel_x;
    uint32_t pixel_y;

    pixel_index = read_packed_sprite_pixel(ctx->mem + data_addr, index, bits_per_pixel);
    if (pixel_index == 0u && transparent_zero)
    {
      continue;
    }

    pixel_x = index & 7u;
    pixel_y = index >> 3u;

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
        decode_guest_color(draw_command_palette_entry(ctx, command, pixel), &red, &green, &blue);
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
    SDL_RenderDrawPoint(renderer, x + (int32_t)pixel_x, y + (int32_t)pixel_y);
  }

  return 1;
}

/**
 * @brief Draws one raw 8x8 tile emitted through vDrawTile().
 */
static int draw_guest_tile(SDL_Renderer *renderer, const VMGPContext *ctx, const MVM_DrawCommand_t *command)
{
  uint32_t raw_format;
  uint32_t format;
  uint32_t bits_per_pixel;
  uint32_t byte_count;
  uint32_t pixel_index;
  uint32_t index;
  uint32_t palette_offset;
  uint8_t pixel;
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  if (!renderer || !ctx || !command || command->aux == 0u)
  {
    return 0;
  }

  raw_format = command->aux2;
  format = raw_format & 0x07u;
  bits_per_pixel = sprite_format_bits_per_pixel((uint8_t)format);
  if (bits_per_pixel == 0u)
  {
    return 0;
  }

  byte_count = (64u * bits_per_pixel + 7u) / 8u;
  if (!MVM_RuntimeMemRangeOk(ctx, command->aux, byte_count))
  {
    return 0;
  }

  palette_offset = (raw_format >> 8) & 0xFFu;

  for (index = 0u; index < 64u; ++index)
  {
    pixel_index = read_packed_sprite_pixel(ctx->mem + command->aux, index, bits_per_pixel);
    if (pixel_index == 0u && (raw_format & 0x08u) != 0u)
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
        pixel = (uint8_t)(palette_offset + pixel_index);
        decode_guest_color(draw_command_palette_entry(ctx, command, pixel), &red, &green, &blue);
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
    SDL_RenderDrawPoint(renderer,
                        command->x0 + (int32_t)(index & 7u),
                        command->y0 + (int32_t)(index >> 3));
  }

  return 1;
}

/**
 * @brief Applies the SDK tilemap auto-animation attribute to one tile index.
 */
static uint8_t apply_guest_map_autoanim(const VMGPMapState *map_state, uint8_t tile_index, uint8_t tile_attribute)
{
  uint8_t animation_bits;
  uint8_t frame_count;

  if (!map_state || (map_state->flags & 0x08u) == 0u)
  {
    return tile_index;
  }

  animation_bits = (uint8_t)(tile_attribute & 0xC0u);
  if (animation_bits == 0u)
  {
    return tile_index;
  }

  frame_count = (animation_bits == 0xC0u) ? 8u : (uint8_t)(animation_bits >> 5);
  return (uint8_t)(tile_index + ((frame_count - 1u) & map_state->animation_active));
}

/**
 * @brief Draws the active VM tilemap using one sprite-atlas attempt plus fallback tiles.
 */
static void render_guest_map(SDL_Renderer *renderer,
                             const VMGPContext *ctx,
                             const MVM_DrawCommand_t *command,
                             const VMGPMapState *map_state)
{
  uint32_t stride;
  uint32_t x;
  uint32_t y;
  uint32_t offset;
  uint8_t tile_index;
  uint8_t tile_attribute;

  if (!renderer || !ctx || !map_state)
  {
    return;
  }

  if (!map_state->valid || map_state->width == 0u || map_state->height == 0u)
  {
    return;
  }

  stride = ((map_state->flags & 0x02u) != 0u) ? 2u : 1u;

  for (y = 0u; y < map_state->height; ++y)
  {
    for (x = 0u; x < map_state->width; ++x)
    {
      offset = map_state->map_data_addr + ((y * (uint32_t)map_state->width) + x) * stride;
      if (!MVM_RuntimeMemRangeOk(ctx, offset, stride))
      {
        continue;
      }

      tile_index = ctx->mem[offset];
      tile_attribute = (stride > 1u) ? ctx->mem[offset + 1u] : 0u;
      tile_index = apply_guest_map_autoanim(map_state, tile_index, tile_attribute);
      draw_guest_map_tile(renderer,
                          ctx,
                          command,
                          map_state,
                          tile_index,
                          tile_attribute,
                          (int32_t)map_state->x_pan + (int32_t)(x * 8u) - (int32_t)map_state->x_pos,
                          (int32_t)map_state->y_pan + (int32_t)(y * 8u) - (int32_t)map_state->y_pos);
    }
  }
}

/**
 * @brief Draws the active VM sprite-slot table as persistent scene state.
 */
static void render_guest_sprite_slots(SDL_Renderer *renderer,
                                      const VMGPContext *ctx,
                                      const MVM_DrawCommand_t *source_command)
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
    if (source_command)
    {
      command.palette_snapshot = source_command->palette_snapshot;
      command.palette_valid = source_command->palette_valid;
      command.clip_x0 = source_command->clip_x0;
      command.clip_y0 = source_command->clip_y0;
      command.clip_x1 = source_command->clip_x1;
      command.clip_y1 = source_command->clip_y1;
    }
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

static void apply_draw_command_clip(SDL_Renderer *renderer, const MVM_DrawCommand_t *command)
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

uint32_t MVM_RenderReplayCommands(MpnVM_t *vm, const MVM_RenderBackend_t *backend, uint32_t first_command)
{
  VMGPContext *ctx;
  const MVM_DrawCommand_t *command;
  SDL_Renderer renderer_state;
  SDL_Rect rect;
  uint32_t index;

  if (!vm || !backend)
  {
    return 0u;
  }

  ctx = (VMGPContext *)vm;
  if (first_command > ctx->draw_command_count)
  {
    first_command = ctx->draw_command_count;
  }

  memset(&renderer_state, 0, sizeof(renderer_state));
  renderer_state.backend = backend;

  for (index = first_command; index < ctx->draw_command_count; ++index)
  {
    command = &ctx->draw_commands[index];

    if (command->type == MVM_DRAW_TEXT)
    {
      continue;
    }

    apply_draw_command_clip(&renderer_state, command);
    set_renderer_guest_color(&renderer_state, command->color);

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
        SDL_RenderFillRect(&renderer_state, &rect);
        break;

      case MVM_DRAW_LINE:
        SDL_RenderDrawLine(&renderer_state, command->x0, command->y0, command->x1, command->y1);
        break;

      case MVM_DRAW_SPRITE:
        if (!draw_guest_sprite(&renderer_state, ctx, command))
        {
          SDL_SetRenderDrawColor(&renderer_state, 255u, 255u, 255u, 255u);
          rect.x = command->x0;
          rect.y = command->y0;
          rect.w = command->width != 0u ? command->width : 8;
          rect.h = command->height != 0u ? command->height : 8;
          SDL_RenderDrawRect(&renderer_state, &rect);
        }
        break;

      case MVM_DRAW_MAP:
        render_guest_map(&renderer_state, ctx, command, &command->map_state);
        break;

      case MVM_DRAW_SPRITE_SLOTS:
        render_guest_sprite_slots(&renderer_state, ctx, command);
        break;

      case MVM_DRAW_TILE:
        (void)draw_guest_tile(&renderer_state, ctx, command);
        break;

      default:
        break;
    }
  }

  for (index = first_command; index < ctx->draw_command_count; ++index)
  {
    command = &ctx->draw_commands[index];
    if (command->type != MVM_DRAW_TEXT)
    {
      continue;
    }

    apply_draw_command_clip(&renderer_state, command);
    set_renderer_guest_color(&renderer_state, command->color);
    if (!draw_guest_text(&renderer_state, ctx, command))
    {
      SDL_SetRenderDrawColor(&renderer_state, 255u, 255u, 0u, 255u);
      rect.x = command->x0;
      rect.y = command->y0;
      rect.w = command->width != 0u ? command->width : 8;
      rect.h = command->height != 0u ? command->height : 8;
      SDL_RenderDrawRect(&renderer_state, &rect);
    }
  }

  SDL_RenderSetClipRect(&renderer_state, NULL);

  return ctx->draw_command_count;
}
