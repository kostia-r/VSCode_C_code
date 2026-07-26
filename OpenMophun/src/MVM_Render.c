#include "MVM_Internal.h"
#include "MVM_SystemFontT230.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

typedef struct MVM_RenderRect_t
{
  int x;
  int y;
  int w;
  int h;
} MVM_RenderRect_t;

typedef struct MVM_RenderBackend_t
{
  void *user;
  void (*set_clip)(void *user, int enabled, int32_t x, int32_t y, int32_t width, int32_t height);
  void (*draw_point)(void *user, int32_t x, int32_t y, uint8_t red, uint8_t green, uint8_t blue);
  void (*draw_line)(void *user,
                    int32_t x0,
                    int32_t y0,
                    int32_t x1,
                    int32_t y1,
                    uint8_t red,
                    uint8_t green,
                    uint8_t blue);
  void (*fill_rect)(void *user,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height,
                    uint8_t red,
                    uint8_t green,
                    uint8_t blue);
} MVM_RenderBackend_t;

typedef struct MVM_SoftwareRenderer_t
{
  const MVM_RenderBackend_t *backend;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} MVM_SoftwareRenderer_t;

/** @brief Stores software-render state for one VM-owned RGB565 framebuffer. */
typedef struct MVM_FramebufferBackend_t
{
  VMGPContext *ctx;       /**< VM owning the destination framebuffer. */
  int32_t clipX0;         /**< Inclusive left clip edge. */
  int32_t clipY0;         /**< Inclusive top clip edge. */
  int32_t clipX1;         /**< Exclusive right clip edge. */
  int32_t clipY1;         /**< Exclusive bottom clip edge. */
  int32_t dirtyX0;        /**< Inclusive left dirty edge. */
  int32_t dirtyY0;        /**< Inclusive top dirty edge. */
  int32_t dirtyX1;        /**< Inclusive right dirty edge. */
  int32_t dirtyY1;        /**< Inclusive bottom dirty edge. */
  uint8_t hasDirty;       /**< Non-zero after at least one destination pixel changes. */
} MVM_FramebufferBackend_t;

static void MVM_lFramebufferSetClip(void *user,
                                    int enabled,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height);
static uint32_t MVM_lReplayCommands(MpnVM_t *vm,
                                    const MVM_RenderBackend_t *backend,
                                    uint32_t first_command);
static void MVM_lConsumeCommands(MpnVM_t *vm);
static void MVM_lFramebufferDrawPoint(void *user,
                                      int32_t x,
                                      int32_t y,
                                      uint8_t red,
                                      uint8_t green,
                                      uint8_t blue);
static void MVM_lFramebufferDrawLine(void *user,
                                     int32_t x0,
                                     int32_t y0,
                                     int32_t x1,
                                     int32_t y1,
                                     uint8_t red,
                                     uint8_t green,
                                     uint8_t blue);
static void MVM_lFramebufferFillRect(void *user,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height,
                                     uint8_t red,
                                     uint8_t green,
                                     uint8_t blue);
static uint16_t MVM_lRgb565(uint8_t red, uint8_t green, uint8_t blue);
static void MVM_lDecodeGuestColor(uint32_t color, uint8_t *red, uint8_t *green, uint8_t *blue);

static void MVM_lSetRenderColor(MVM_SoftwareRenderer_t *renderer, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
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

static void MVM_lDrawPoint(MVM_SoftwareRenderer_t *renderer, int32_t x, int32_t y)
{
  if (renderer && renderer->backend && renderer->backend->draw_point)
  {
    renderer->backend->draw_point(renderer->backend->user, x, y, renderer->red, renderer->green, renderer->blue);
  }
}

static void MVM_lDrawLine(MVM_SoftwareRenderer_t *renderer, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  if (renderer && renderer->backend && renderer->backend->draw_line)
  {
    renderer->backend->draw_line(renderer->backend->user, x0, y0, x1, y1, renderer->red, renderer->green, renderer->blue);
  }
}

static void MVM_lFillRect(MVM_SoftwareRenderer_t *renderer, const MVM_RenderRect_t *rect)
{
  if (renderer && renderer->backend && renderer->backend->fill_rect && rect)
  {
    renderer->backend->fill_rect(renderer->backend->user, rect->x, rect->y, rect->w, rect->h, renderer->red, renderer->green, renderer->blue);
  }
}

static void MVM_lDrawRect(MVM_SoftwareRenderer_t *renderer, const MVM_RenderRect_t *rect)
{
  if (!renderer || !rect)
  {
    return;
  }

  MVM_lDrawLine(renderer, rect->x, rect->y, rect->x + rect->w - 1, rect->y);
  MVM_lDrawLine(renderer, rect->x, rect->y, rect->x, rect->y + rect->h - 1);
  MVM_lDrawLine(renderer, rect->x + rect->w - 1, rect->y, rect->x + rect->w - 1, rect->y + rect->h - 1);
  MVM_lDrawLine(renderer, rect->x, rect->y + rect->h - 1, rect->x + rect->w - 1, rect->y + rect->h - 1);
}

static void MVM_lSetClipRect(MVM_SoftwareRenderer_t *renderer, const MVM_RenderRect_t *rect)
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

static void MVM_lConsumeCommands(MpnVM_t *vm)
{
  VMGPContext *ctx;

  if (!vm)
  {
    return;
  }

  ctx = (VMGPContext *)vm;
  ctx->draw_command_count = 0u;
  ctx->draw_palette_count = 0u;
} /* End of MVM_lConsumeCommands */

int MVM_RenderApplyPendingFramebuffer(MpnVM_t *vm)
{
  VMGPContext *ctx;
  MVM_FramebufferBackend_t framebufferBackend;
  MVM_RenderBackend_t renderBackend;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint16_t clearPixel;
  size_t pixelCount;
  size_t pixelIndex;

  if (!vm)
  {
    return 0;
  }

  ctx = (VMGPContext *)vm;
  if (!ctx->driver_framebuffer ||
      ctx->driver_framebuffer_width == 0U || ctx->driver_framebuffer_height == 0U)
  {
    return 0;
  }

  memset(&framebufferBackend, 0, sizeof(framebufferBackend));
  framebufferBackend.ctx = ctx;
  framebufferBackend.clipX1 = ctx->driver_framebuffer_width;
  framebufferBackend.clipY1 = ctx->driver_framebuffer_height;
  framebufferBackend.hasDirty = ctx->driver_framebuffer_dirty;
  framebufferBackend.dirtyX0 = ctx->driver_dirty_x0;
  framebufferBackend.dirtyY0 = ctx->driver_dirty_y0;
  framebufferBackend.dirtyX1 = ctx->driver_dirty_x1;
  framebufferBackend.dirtyY1 = ctx->driver_dirty_y1;

  if (ctx->driver_framebuffer_clear_serial != ctx->clear_serial)
  {
    MVM_lDecodeGuestColor(ctx->clear_color, &red, &green, &blue);
    clearPixel = MVM_lRgb565(red, green, blue);
    pixelCount = (size_t)ctx->driver_framebuffer_width * ctx->driver_framebuffer_height;
    for (pixelIndex = 0U; pixelIndex < pixelCount; ++pixelIndex)
    {
      ctx->driver_framebuffer[pixelIndex] = clearPixel;
    } /* End of loop */

    framebufferBackend.hasDirty = 1U;
    framebufferBackend.dirtyX1 = (int32_t)ctx->driver_framebuffer_width - 1;
    framebufferBackend.dirtyY1 = (int32_t)ctx->driver_framebuffer_height - 1;
    ctx->driver_framebuffer_clear_serial = ctx->clear_serial;
  }

  renderBackend.user = &framebufferBackend;
  renderBackend.set_clip = MVM_lFramebufferSetClip;
  renderBackend.draw_point = MVM_lFramebufferDrawPoint;
  renderBackend.draw_line = MVM_lFramebufferDrawLine;
  renderBackend.fill_rect = MVM_lFramebufferFillRect;
  (void)MVM_lReplayCommands(vm, &renderBackend, 0U);
  MVM_lConsumeCommands(vm);

  ctx->driver_framebuffer_dirty = framebufferBackend.hasDirty;
  ctx->driver_dirty_x0 = (int16_t)framebufferBackend.dirtyX0;
  ctx->driver_dirty_y0 = (int16_t)framebufferBackend.dirtyY0;
  ctx->driver_dirty_x1 = (int16_t)framebufferBackend.dirtyX1;
  ctx->driver_dirty_y1 = (int16_t)framebufferBackend.dirtyY1;

  return 1;
} /* End of MVM_RenderApplyPendingFramebuffer */

int MVM_RenderFlushFramebuffer(MpnVM_t *vm)
{
  VMGPContext *ctx;
  MVM_Framebuffer_t framebuffer;
  int result;

  if (!vm)
  {
    return 0;
  }

  ctx = (VMGPContext *)vm;
  if (!ctx->drivers.display_flush || !MVM_RenderApplyPendingFramebuffer(vm))
  {
    return 0;
  }

  if (!ctx->driver_framebuffer_dirty)
  {
    return 1;
  }

  framebuffer.pixels = ctx->driver_framebuffer;
  framebuffer.width = ctx->driver_framebuffer_width;
  framebuffer.height = ctx->driver_framebuffer_height;
  framebuffer.stride_bytes = (uint32_t)ctx->driver_framebuffer_width * sizeof(uint16_t);
  framebuffer.pixel_format = MVM_PIXEL_FORMAT_RGB565;
  framebuffer.dirty_rect.x = (uint16_t)ctx->driver_dirty_x0;
  framebuffer.dirty_rect.y = (uint16_t)ctx->driver_dirty_y0;
  framebuffer.dirty_rect.width = (uint16_t)(ctx->driver_dirty_x1 - ctx->driver_dirty_x0 + 1);
  framebuffer.dirty_rect.height = (uint16_t)(ctx->driver_dirty_y1 - ctx->driver_dirty_y0 + 1);

  result = ctx->drivers.display_flush(ctx->drivers.context, &framebuffer);
  if (result == 0)
  {
    ctx->driver_framebuffer_dirty = 0U;
  }

  return result == 0;
} /* End of MVM_RenderFlushFramebuffer */

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
static uint32_t draw_command_palette_entry(const VMGPContext *ctx,
                                           const MVM_DrawCommand_t *command,
                                           uint8_t index);
static void draw_filled_triangle(MVM_SoftwareRenderer_t *renderer,
                                 int16_t x0,
                                 int16_t y0,
                                 int16_t x1,
                                 int16_t y1,
                                 int16_t x2,
                                 int16_t y2);

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
    pixel_index = read_packed_sprite_pixel(MVM_GUEST_CONST_PTR(ctx, sprite->data_addr, byte_count),
                                           index,
                                           bits_per_pixel);

    if (pixel_index != 0u)
    {
      ++non_zero_count;
    }
  }

  return (non_zero_count * 1024u) / pixel_count;
}

/**
 * @brief Mirrors one minimal guest FONT header layout for software text rendering.
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

  font->font_data_addr = vm_read_u32_le(MVM_GUEST_PTR(ctx, font_addr + 0u, 4u));
  font->char_table_addr = vm_read_u32_le(MVM_GUEST_PTR(ctx, font_addr + 4u, 4u));
  font->bpp = MVM_GUEST_BYTE(ctx, font_addr + 8u);
  font->width = MVM_GUEST_BYTE(ctx, font_addr + 9u);
  font->height = MVM_GUEST_BYTE(ctx, font_addr + 10u);
  font->palindex = MVM_GUEST_BYTE(ctx, font_addr + 11u);

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
 * @brief Applies one guest-encoded color to the active software renderer.
 */
static void decode_draw_command_color(const VMGPContext *ctx,
                                      const MVM_DrawCommand_t *command,
                                      uint32_t color,
                                      uint8_t *red,
                                      uint8_t *green,
                                      uint8_t *blue)
{
  if ((color & 0x80000000u) != 0u)
  {
    decode_guest_color(color, red, green, blue);
  }
  else
  {
    decode_guest_color(draw_command_palette_entry(ctx, command, (uint8_t)(color > 0xFFu ? 0xFFu : color)), red, green, blue);
  }
}

static void set_renderer_guest_color(MVM_SoftwareRenderer_t *renderer,
                                     const VMGPContext *ctx,
                                     const MVM_DrawCommand_t *command,
                                     uint32_t color)
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  if (!renderer)
  {
    return;
  }

  decode_draw_command_color(ctx, command, color, &red, &green, &blue);
  MVM_lSetRenderColor(renderer, red, green, blue, 255u);
}

/**
 * @brief Reads one guest SPRITE header for the minimal software renderer.
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

  sprite->palindex = MVM_GUEST_BYTE(ctx, sprite_addr + 0u);
  sprite->format = MVM_GUEST_BYTE(ctx, sprite_addr + 1u);
  sprite->center_x = (int16_t)vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 2u, 2u));
  sprite->center_y = (int16_t)vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 4u, 2u));
  sprite->width = vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 6u, 2u));
  sprite->height = vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 8u, 2u));
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

  word0 = vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 0u, 2u));
  word1 = vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 2u, 2u));
  word2 = vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 4u, 2u));
  word3 = vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 6u, 2u));
  word4 = vm_read_u16_le(MVM_GUEST_PTR(ctx, sprite_addr + 8u, 2u));
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
 * @brief Reads one guest FONT header for the minimal software renderer.
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

static int32_t triangle_edge(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t px, int32_t py)
{
  return ((px - ax) * (by - ay)) - ((py - ay) * (bx - ax));
}

static int16_t min3_i16(int16_t a, int16_t b, int16_t c)
{
  int16_t result;

  result = a < b ? a : b;
  return result < c ? result : c;
}

static int16_t max3_i16(int16_t a, int16_t b, int16_t c)
{
  int16_t result;

  result = a > b ? a : b;
  return result > c ? result : c;
}

static void draw_filled_triangle(MVM_SoftwareRenderer_t *renderer,
                                 int16_t x0,
                                 int16_t y0,
                                 int16_t x1,
                                 int16_t y1,
                                 int16_t x2,
                                 int16_t y2)
{
  int32_t min_x;
  int32_t max_x;
  int32_t min_y;
  int32_t max_y;
  int32_t area;
  int32_t x;
  int32_t y;

  if (!renderer)
  {
    return;
  }

  min_x = min3_i16(x0, x1, x2);
  max_x = max3_i16(x0, x1, x2);
  min_y = min3_i16(y0, y1, y2);
  max_y = max3_i16(y0, y1, y2);
  area = triangle_edge(x0, y0, x1, y1, x2, y2);

  if (area == 0)
  {
    MVM_lDrawLine(renderer, x0, y0, x1, y1);
    MVM_lDrawLine(renderer, x1, y1, x2, y2);
    MVM_lDrawLine(renderer, x2, y2, x0, y0);
    return;
  }

  for (y = min_y; y <= max_y; ++y)
  {
    int32_t span_start;
    int32_t span_end;

    span_start = max_x + 1;
    span_end = min_x - 1;

    for (x = min_x; x <= max_x; ++x)
    {
      int32_t w0;
      int32_t w1;
      int32_t w2;

      w0 = triangle_edge(x1, y1, x2, y2, x, y);
      w1 = triangle_edge(x2, y2, x0, y0, x, y);
      w2 = triangle_edge(x0, y0, x1, y1, x, y);

      if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
          (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0))
      {
        if (x < span_start)
        {
          span_start = x;
        }
        span_end = x;
      }
    }

    if (span_start <= span_end)
    {
      MVM_lDrawLine(renderer, span_start, y, span_end, y);
    }
  }
}

/**
 * @brief Draws one guest SPRITE using a minimal RGB332 software path.
 */
static int draw_guest_sprite(MVM_SoftwareRenderer_t *renderer, const VMGPContext *ctx, const MVM_DrawCommand_t *command)
{
  VmSpriteHeader sprite;
  uint32_t byte_index;
  uint32_t pixels_per_byte;
  uint32_t pixel_index;
  uint32_t pixel_count;
  uint32_t index;
  uint32_t data_addr;
  uint32_t pixel_x;
  uint32_t pixel_y;
  int32_t x;
  int32_t y;
  uint32_t pixel;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint32_t bits_per_pixel;
  int transparent_zero;
  int flip_x;
  int flip_y;

  if (!renderer || !ctx || !command)
  {
    return 0;
  }

  if (!read_guest_sprite_header(ctx, command->aux, &sprite))
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

  transparent_zero = ((command->transfer_mode & 0x01u) != 0u);
  flip_x = ((command->transfer_mode & 0x02u) != 0u);
  flip_y = ((command->transfer_mode & 0x04u) != 0u);

  for (index = 0u; index < pixel_count; ++index)
  {
    pixel_index = read_packed_sprite_pixel(MVM_GUEST_CONST_PTR(ctx, data_addr, byte_index),
                                           index,
                                           bits_per_pixel);

    if (pixel_index == 0u && transparent_zero)
    {
      continue;
    }

    pixel_x = index % sprite.width;
    pixel_y = index / sprite.width;
    if (flip_x)
    {
      pixel_x = (uint32_t)sprite.width - 1u - pixel_x;
    }
    if (flip_y)
    {
      pixel_y = (uint32_t)sprite.height - 1u - pixel_y;
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
        pixel = (uint32_t)sprite.palindex + pixel_index;
        if (pixel >= 256u)
        {
          continue;
        }
        decode_guest_color(draw_command_palette_entry(ctx, command, pixel), &red, &green, &blue);
        break;

      case 0x07u:
        pixel = pixel_index;
        red = (uint8_t)(((pixel >> 5) & 0x07u) * 255u / 7u);
        green = (uint8_t)(((pixel >> 2) & 0x07u) * 255u / 7u);
        blue = (uint8_t)((pixel & 0x03u) * 255u / 3u);
        break;

      default:
        return 0;
    }

    x = command->x0 - (int32_t)sprite.center_x + (int32_t)pixel_x;
    y = command->y0 - (int32_t)sprite.center_y + (int32_t)pixel_y;

    MVM_lSetRenderColor(renderer, red, green, blue, 255u);
    MVM_lDrawPoint(renderer, x, y);
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
  static const uint8_t GLYPH_a[7]     = { 0x00u, 0x00u, 0x0Eu, 0x01u, 0x0Fu, 0x11u, 0x0Fu };
  static const uint8_t GLYPH_b[7]     = { 0x10u, 0x10u, 0x1Eu, 0x11u, 0x11u, 0x11u, 0x1Eu };
  static const uint8_t GLYPH_c[7]     = { 0x00u, 0x00u, 0x0Eu, 0x10u, 0x10u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_d[7]     = { 0x01u, 0x01u, 0x0Fu, 0x11u, 0x11u, 0x11u, 0x0Fu };
  static const uint8_t GLYPH_e[7]     = { 0x00u, 0x00u, 0x0Eu, 0x11u, 0x1Fu, 0x10u, 0x0Eu };
  static const uint8_t GLYPH_f[7]     = { 0x06u, 0x09u, 0x08u, 0x1Cu, 0x08u, 0x08u, 0x08u };
  static const uint8_t GLYPH_g[7]     = { 0x00u, 0x00u, 0x0Fu, 0x11u, 0x0Fu, 0x01u, 0x0Eu };
  static const uint8_t GLYPH_h[7]     = { 0x10u, 0x10u, 0x1Eu, 0x11u, 0x11u, 0x11u, 0x11u };
  static const uint8_t GLYPH_i[7]     = { 0x04u, 0x00u, 0x0Cu, 0x04u, 0x04u, 0x04u, 0x0Eu };
  static const uint8_t GLYPH_j[7]     = { 0x02u, 0x00u, 0x06u, 0x02u, 0x02u, 0x12u, 0x0Cu };
  static const uint8_t GLYPH_k[7]     = { 0x10u, 0x10u, 0x12u, 0x14u, 0x18u, 0x14u, 0x12u };
  static const uint8_t GLYPH_l[7]     = { 0x0Cu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x0Eu };
  static const uint8_t GLYPH_m[7]     = { 0x00u, 0x00u, 0x1Au, 0x15u, 0x15u, 0x15u, 0x15u };
  static const uint8_t GLYPH_n[7]     = { 0x00u, 0x00u, 0x1Eu, 0x11u, 0x11u, 0x11u, 0x11u };
  static const uint8_t GLYPH_o[7]     = { 0x00u, 0x00u, 0x0Eu, 0x11u, 0x11u, 0x11u, 0x0Eu };
  static const uint8_t GLYPH_p[7]     = { 0x00u, 0x00u, 0x1Eu, 0x11u, 0x1Eu, 0x10u, 0x10u };
  static const uint8_t GLYPH_q[7]     = { 0x00u, 0x00u, 0x0Fu, 0x11u, 0x0Fu, 0x01u, 0x01u };
  static const uint8_t GLYPH_r[7]     = { 0x00u, 0x00u, 0x16u, 0x18u, 0x10u, 0x10u, 0x10u };
  static const uint8_t GLYPH_s[7]     = { 0x00u, 0x00u, 0x0Fu, 0x10u, 0x0Eu, 0x01u, 0x1Eu };
  static const uint8_t GLYPH_t[7]     = { 0x08u, 0x08u, 0x1Eu, 0x08u, 0x08u, 0x09u, 0x06u };
  static const uint8_t GLYPH_u[7]     = { 0x00u, 0x00u, 0x11u, 0x11u, 0x11u, 0x13u, 0x0Du };
  static const uint8_t GLYPH_v[7]     = { 0x00u, 0x00u, 0x11u, 0x11u, 0x11u, 0x0Au, 0x04u };
  static const uint8_t GLYPH_w[7]     = { 0x00u, 0x00u, 0x11u, 0x15u, 0x15u, 0x15u, 0x0Au };
  static const uint8_t GLYPH_x[7]     = { 0x00u, 0x00u, 0x11u, 0x0Au, 0x04u, 0x0Au, 0x11u };
  static const uint8_t GLYPH_y[7]     = { 0x00u, 0x00u, 0x11u, 0x11u, 0x0Fu, 0x01u, 0x0Eu };
  static const uint8_t GLYPH_z[7]     = { 0x00u, 0x00u, 0x1Fu, 0x02u, 0x04u, 0x08u, 0x1Fu };

  switch ((uint8_t)ch)
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
    case 'a': return GLYPH_a;
    case 'b': return GLYPH_b;
    case 'c': return GLYPH_c;
    case 'd': return GLYPH_d;
    case 'e': return GLYPH_e;
    case 'f': return GLYPH_f;
    case 'g': return GLYPH_g;
    case 'h': return GLYPH_h;
    case 'i': return GLYPH_i;
    case 'j': return GLYPH_j;
    case 'k': return GLYPH_k;
    case 'l': return GLYPH_l;
    case 'm': return GLYPH_m;
    case 'n': return GLYPH_n;
    case 'o': return GLYPH_o;
    case 'p': return GLYPH_p;
    case 'q': return GLYPH_q;
    case 'r': return GLYPH_r;
    case 's': return GLYPH_s;
    case 't': return GLYPH_t;
    case 'u': return GLYPH_u;
    case 'v': return GLYPH_v;
    case 'w': return GLYPH_w;
    case 'x': return GLYPH_x;
    case 'y': return GLYPH_y;
    case 'z': return GLYPH_z;
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
 * @brief Draws one byte string with the tiny built-in debug fallback font.
 */
static int draw_debug_text_bytes(MVM_SoftwareRenderer_t *renderer,
                                 const VMGPContext *ctx,
                                 const uint8_t *text,
                                 uint32_t char_count,
                                 const uint32_t *palette,
                                 uint32_t color,
                                 uint32_t str_addr,
                                 int32_t x,
                                 int32_t y)
{
  const int32_t char_advance = 8;
  uint32_t char_index;
  uint32_t row;
  uint32_t col;
  const uint8_t *glyph;
  uint8_t ch;
  uint8_t fg_red;
  uint8_t fg_green;
  uint8_t fg_blue;

  if (!renderer || !ctx || !text)
  {
    return 0;
  }

  decode_guest_color(color != 0u ? color : (palette ? palette[1] : 0xFFu), &fg_red, &fg_green, &fg_blue);

  for (char_index = 0u; char_index < char_count; ++char_index)
  {
    ch = decode_debug_text_char(ctx, str_addr, char_index, text[char_index]);
    glyph = debug_font_glyph(ch);

    for (row = 0u; row < 7u; ++row)
    {
      for (col = 0u; col < 5u; ++col)
      {
        if ((glyph[row] & (1u << (4u - col))) != 0u)
        {
          MVM_lSetRenderColor(renderer, fg_red, fg_green, fg_blue, 255u);
          MVM_lDrawPoint(renderer,
                              x + (int32_t)(char_index * (uint32_t)char_advance) + (int32_t)col,
                              y + (int32_t)row);
        }
      }
    }
  }

  return 1;
}

static int draw_system_text_bytes(MVM_SoftwareRenderer_t *renderer,
                                  const VMGPContext *ctx,
                                  const uint8_t *text,
                                  uint32_t char_count,
                                  const uint32_t *palette,
                                   uint32_t color,
                                   uint32_t str_addr,
                                   uint32_t font_size,
                                   uint32_t font_flags,
                                  int32_t x,
                                  int32_t y)
{
  const MVM_SystemFontFace_t *face;
  const int outline = ((font_flags & (1u << 27)) != 0u);
  const uint32_t shadow_effect = font_flags & 0xF8000000u;
  uint32_t char_index;
  uint32_t row;
  uint32_t col;
  uint8_t ch;
  uint8_t fg_red;
  uint8_t fg_green;
  uint8_t fg_blue;
  uint8_t shadow_red;
  uint8_t shadow_green;
  uint8_t shadow_blue;
  int32_t pen_x;

  if (!renderer || !ctx || !text)
  {
    return 0;
  }

  decode_guest_color(color != 0u ? color : (palette ? palette[1] : 0xFFu), &fg_red, &fg_green, &fg_blue);
  shadow_red = 0u;
  shadow_green = 0u;
  shadow_blue = 0u;
  pen_x = x;
  face = (font_size == 1u) ? &MVM_SystemFontFaceSmallCandidate : &MVM_SystemFontFaceNormalPlaceholder;

  for (char_index = 0u; char_index < char_count; ++char_index)
  {
    uint8_t raw_ch;

    raw_ch = text[char_index];
    ch = decode_debug_text_char(ctx, str_addr, char_index, raw_ch);

    if (raw_ch == 0x01u)
    {
      const int32_t glyph_x = pen_x;

      if (shadow_effect != 0u || outline)
      {
        MVM_lSetRenderColor(renderer, shadow_red, shadow_green, shadow_blue, 255u);
        MVM_lDrawLine(renderer, glyph_x + 1, y + 1, glyph_x + 1, y + 7);
        MVM_lDrawLine(renderer, glyph_x + 5, y + 1, glyph_x + 5, y + 7);
        MVM_lDrawLine(renderer, glyph_x + 1, y + 1, glyph_x + 5, y + 7);
      }
      MVM_lSetRenderColor(renderer, fg_red, fg_green, fg_blue, 255u);
      MVM_lDrawLine(renderer, glyph_x + 0, y + 0, glyph_x + 0, y + 6);
      MVM_lDrawLine(renderer, glyph_x + 4, y + 0, glyph_x + 4, y + 6);
      MVM_lDrawLine(renderer, glyph_x + 0, y + 0, glyph_x + 4, y + 6);
      pen_x += 8;
      continue;
    }

    if (ch < 0x20u || ch > 0x7Eu)
    {
      pen_x += 8;
      continue;
    }

    for (row = 0u; row < face->glyphs[ch - 0x20u].height; ++row)
    {
      uint16_t row_bits;
      int32_t span_start;

      row_bits = face->glyphs[ch - 0x20u].rows[row];
      span_start = -1;
      for (col = 0u; col <= face->glyphs[ch - 0x20u].width; ++col)
      {
        const int bit_set = (col < face->glyphs[ch - 0x20u].width) &&
                            ((row_bits & (1u << col)) != 0u);

        if (bit_set && span_start < 0)
        {
          span_start = (int32_t)col;
        }
        else if (!bit_set && span_start >= 0)
        {
            const int32_t glyph_y = y + (int32_t)face->glyphs[ch - 0x20u].top + (int32_t)row;
          const int32_t span_end = (int32_t)col - 1;

          if (shadow_effect != 0u || outline)
          {
            MVM_lSetRenderColor(renderer, shadow_red, shadow_green, shadow_blue, 255u);
            MVM_lDrawLine(renderer, pen_x + span_start + 1, glyph_y + 1, pen_x + span_end + 1, glyph_y + 1);
          }
          MVM_lSetRenderColor(renderer, fg_red, fg_green, fg_blue, 255u);
          MVM_lDrawLine(renderer, pen_x + span_start, glyph_y, pen_x + span_end, glyph_y);
          span_start = -1;
        }
      }
    }

    pen_x += face->glyphs[ch - 0x20u].advance;
  }

  return 1;
}

/**
 * @brief Draws one guest text string using the active VM font data.
 */
static int draw_guest_text(MVM_SoftwareRenderer_t *renderer, const VMGPContext *ctx, const MVM_DrawCommand_t *command)
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
  uint32_t text_color;
  const uint8_t *text_bytes;

  if (!renderer || !ctx || !command)
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
  else if (MVM_RuntimeMemRangeOk(ctx, str_addr, 1u))
  {
    text_bytes = MVM_GUEST_CONST_PTR(ctx, str_addr, 1u);
    char_count = MVM_RuntimeStrLen(text_bytes, MVM_GuestContiguousSize(ctx, str_addr));
  }

  if (command->aux2 == 0u)
  {
    return draw_system_text_bytes(renderer,
                                  ctx,
                                  text_bytes,
                                  char_count,
                                  command->text_palette,
                                  command->color,
                                  str_addr,
                                  command->system_font_size,
                                  command->system_font_flags,
                                  command->x0,
                                  command->y0);
  }

  if (!read_guest_font_header(ctx, command->aux2, &font))
  {
    return draw_debug_text_bytes(renderer,
                                 ctx,
                                 text_bytes,
                                 char_count,
                                 command->text_palette,
                                 command->color,
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
                                 command->color,
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
  text_color = command->color != 0u ? command->color : command->text_palette[1];
  decode_draw_command_color(ctx, command, text_color, &fg_red, &fg_green, &fg_blue);
  decode_guest_color(command->text_palette[0], &bg_red, &bg_green, &bg_blue);

  for (char_index = 0u; char_index < char_count; ++char_index)
  {
    ch = text_bytes[char_index];
    if (!MVM_RuntimeMemRangeOk(ctx, font.char_table_addr + ch, 1u))
    {
      continue;
    }

    glyph_index = MVM_GUEST_BYTE(ctx, font.char_table_addr + ch);
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

    glyph_data = MVM_GUEST_CONST_PTR(ctx, glyph_data_addr, bytes_per_char);

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
      if (font.bpp == 1u)
      {
        red = fg_red;
        green = fg_green;
        blue = fg_blue;
      }
      else if ((uint32_t)font.palindex + pixel_value < 4u)
      {
        uint32_t layer_color;

        layer_color = command->text_palette[(uint32_t)font.palindex + pixel_value];
        decode_guest_color(layer_color, &red, &green, &blue);
        if (command->color != 0u)
        {
          uint32_t tint_weight;

          if (pixel_value == max_pixel_value)
          {
            tint_weight = 128u;
          }
          else if ((pixel_value + 1u) == max_pixel_value)
          {
            tint_weight = 64u;
          }
          else
          {
            tint_weight = 0u;
          }

          if (tint_weight != 0u)
          {
            red = (uint8_t)((((uint32_t)red * (255u - tint_weight)) + ((uint32_t)fg_red * tint_weight)) / 255u);
            green = (uint8_t)((((uint32_t)green * (255u - tint_weight)) + ((uint32_t)fg_green * tint_weight)) / 255u);
            blue = (uint8_t)((((uint32_t)blue * (255u - tint_weight)) + ((uint32_t)fg_blue * tint_weight)) / 255u);
          }
        }
      }
      else
      {
        red = (uint8_t)((((max_pixel_value - pixel_value) * bg_red) + (pixel_value * fg_red)) / max_pixel_value);
        green = (uint8_t)((((max_pixel_value - pixel_value) * bg_green) + (pixel_value * fg_green)) / max_pixel_value);
        blue = (uint8_t)((((max_pixel_value - pixel_value) * bg_blue) + (pixel_value * fg_blue)) / max_pixel_value);
      }
      MVM_lSetRenderColor(renderer, red, green, blue, 255u);
      MVM_lDrawPoint(renderer, draw_x, draw_y);
      ++drawn_pixels;
    }
  }

  if (drawn_pixels == 0u)
  {
#if (MVM_COMPILED_LOG_LEVEL >= 4U)
    static uint32_t fallback_log_count = 0u;

    if (fallback_log_count < 16u)
    {
      uint32_t sample_index;
      uint8_t sample_ch;
      uint8_t sample_glyph;
      uint32_t sample_addr;
      uint32_t sample_pixel_lsb;
      uint32_t sample_pixel_msb;

      sample_index = 0u;
      sample_ch = 0u;
      sample_glyph = 0xFFu;
      sample_addr = 0u;
      sample_pixel_lsb = 0u;
      sample_pixel_msb = 0u;

      while (sample_index < char_count && text_bytes[sample_index] == (uint8_t)' ')
      {
        ++sample_index;
      }

      if (sample_index < char_count)
      {
        sample_ch = text_bytes[sample_index];
        if (MVM_RuntimeMemRangeOk(ctx, font.char_table_addr + sample_ch, 1u))
        {
          sample_glyph = MVM_GUEST_BYTE(ctx, font.char_table_addr + sample_ch);
          sample_addr = font.font_data_addr + ((uint32_t)sample_glyph * bytes_per_char_tight);
          if (sample_glyph != 0xFFu && MVM_RuntimeMemRangeOk(ctx, sample_addr, bytes_per_char_tight))
          {
            sample_pixel_lsb = read_tight_font_pixel(MVM_GUEST_CONST_PTR(ctx,
                                                                         sample_addr,
                                                                         bytes_per_char_tight),
                                                     0u,
                                                     (uint32_t)font.bpp,
                                                     0);
            sample_pixel_msb = read_tight_font_pixel(MVM_GUEST_CONST_PTR(ctx,
                                                                         sample_addr,
                                                                         bytes_per_char_tight),
                                                     0u,
                                                     (uint32_t)font.bpp,
                                                     1);
          }
        }
      }

      MVM_LOG_D(ctx,
                "text-render",
                "fallback font=%08X data=%08X table=%08X bpp=%u w=%u h=%u pal=%u len=%u color=%08X sample='%c' code=%02X glyph=%02X addr=%08X pix0-lsb=%u pix0-msb=%u\n",
                command->aux2,
                font.font_data_addr,
                font.char_table_addr,
                font.bpp,
                font.width,
                font.height,
                font.palindex,
                char_count,
                command->color,
                (sample_ch >= 0x20u && sample_ch < 0x7Fu) ? (char)sample_ch : '.',
                sample_ch,
                sample_glyph,
                sample_addr,
                sample_pixel_lsb,
                sample_pixel_msb);
      ++fallback_log_count;
    }
#endif

    return draw_debug_text_bytes(renderer,
                                 ctx,
                                 text_bytes,
                                 char_count,
                                 command->text_palette,
                                 command->color,
                                 str_addr,
                                 command->x0,
                                 command->y0);
  }

  return 1;
}

/**
 * @brief Tries to draw one 8x8 tile from one SDK MAP_HEADER tile-data area.
 */
static int draw_guest_map_tile(MVM_SoftwareRenderer_t *renderer,
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

    pixel_index = read_packed_sprite_pixel(MVM_GUEST_CONST_PTR(ctx, data_addr, byte_count),
                                           index,
                                           bits_per_pixel);
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

    MVM_lSetRenderColor(renderer, red, green, blue, 255u);
    MVM_lDrawPoint(renderer, x + (int32_t)pixel_x, y + (int32_t)pixel_y);
  }

  return 1;
}

/**
 * @brief Draws one raw 8x8 tile emitted through vDrawTile().
 */
static int draw_guest_tile(MVM_SoftwareRenderer_t *renderer, const VMGPContext *ctx, const MVM_DrawCommand_t *command)
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
    pixel_index = read_packed_sprite_pixel(MVM_GUEST_CONST_PTR(ctx, command->aux, byte_count),
                                           index,
                                           bits_per_pixel);
    if (pixel_index == 0u && (((raw_format & 0x08u) != 0u) || ((command->transfer_mode & 0x01u) != 0u)))
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

    MVM_lSetRenderColor(renderer, red, green, blue, 255u);
    MVM_lDrawPoint(renderer,
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
static void render_guest_map(MVM_SoftwareRenderer_t *renderer,
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

      tile_index = MVM_GUEST_BYTE(ctx, offset);
      tile_attribute = (stride > 1u) ? MVM_GUEST_BYTE(ctx, offset + 1u) : 0u;
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
static void render_guest_sprite_slots(MVM_SoftwareRenderer_t *renderer,
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
      command.transfer_mode = source_command->transfer_mode;
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

static void apply_draw_command_clip(MVM_SoftwareRenderer_t *renderer, const MVM_DrawCommand_t *command)
{
  MVM_RenderRect_t clip_rect;

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
    MVM_lSetClipRect(renderer, &clip_rect);
  }
  else
  {
    MVM_lSetClipRect(renderer, NULL);
  }
}

static uint32_t MVM_lReplayCommands(MpnVM_t *vm,
                                    const MVM_RenderBackend_t *backend,
                                    uint32_t first_command)
{
  VMGPContext *ctx;
  const MVM_DrawCommand_t *command;
  MVM_SoftwareRenderer_t renderer_state;
  MVM_RenderRect_t rect;
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

    apply_draw_command_clip(&renderer_state, command);
    set_renderer_guest_color(&renderer_state, ctx, command, command->color);

    switch (command->type)
    {
      case MVM_DRAW_FILL_RECT:
      {
        if (command->x0 >= command->x1)
        {
          break;
        }

        rect.x = command->x0;
        rect.y = command->y0;
        rect.w = command->x1 - command->x0 + 1;
        rect.h = (command->y0 <= command->y1) ? (command->y1 - command->y0 + 1) : 1;
        MVM_lFillRect(&renderer_state, &rect);
        break;
      }

      case MVM_DRAW_LINE:
        MVM_lDrawLine(&renderer_state, command->x0, command->y0, command->x1, command->y1);
        break;

      case MVM_DRAW_TRIANGLE:
        draw_filled_triangle(&renderer_state,
                             command->x0,
                             command->y0,
                             command->x1,
                             command->y1,
                             (int16_t)(command->aux & 0xFFFFu),
                             (int16_t)(command->aux2 & 0xFFFFu));
        break;

      case MVM_DRAW_SPRITE:
        if (!draw_guest_sprite(&renderer_state, ctx, command))
        {
          MVM_lSetRenderColor(&renderer_state, 255u, 255u, 255u, 255u);
          rect.x = command->x0;
          rect.y = command->y0;
          rect.w = command->width != 0u ? command->width : 8;
          rect.h = command->height != 0u ? command->height : 8;
          MVM_lDrawRect(&renderer_state, &rect);
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

      case MVM_DRAW_TEXT:
        if (!draw_guest_text(&renderer_state, ctx, command))
        {
          MVM_lSetRenderColor(&renderer_state, 255u, 255u, 0u, 255u);
          rect.x = command->x0;
          rect.y = command->y0;
          rect.w = command->width != 0u ? command->width : 8;
          rect.h = command->height != 0u ? command->height : 8;
          MVM_lDrawRect(&renderer_state, &rect);
        }
        break;

      default:
        break;
    }
  }

  MVM_lSetClipRect(&renderer_state, NULL);

  return ctx->draw_command_count;
}

static void MVM_lFramebufferSetClip(void *user,
                                    int enabled,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height)
{
  MVM_FramebufferBackend_t *backend = (MVM_FramebufferBackend_t *)user;

  if (!backend || !backend->ctx)
  {
    return;
  }

  if (!enabled)
  {
    backend->clipX0 = 0;
    backend->clipY0 = 0;
    backend->clipX1 = backend->ctx->driver_framebuffer_width;
    backend->clipY1 = backend->ctx->driver_framebuffer_height;
    return;
  }

  backend->clipX0 = (x > 0) ? x : 0;
  backend->clipY0 = (y > 0) ? y : 0;
  backend->clipX1 = x + width;
  backend->clipY1 = y + height;

  if (backend->clipX1 > backend->ctx->driver_framebuffer_width)
  {
    backend->clipX1 = backend->ctx->driver_framebuffer_width;
  }

  if (backend->clipY1 > backend->ctx->driver_framebuffer_height)
  {
    backend->clipY1 = backend->ctx->driver_framebuffer_height;
  }

  if (width <= 0 || height <= 0 || backend->clipX1 < backend->clipX0 || backend->clipY1 < backend->clipY0)
  {
    backend->clipX1 = backend->clipX0;
    backend->clipY1 = backend->clipY0;
  }
} /* End of MVM_lFramebufferSetClip */

static void MVM_lFramebufferDrawPoint(void *user,
                                      int32_t x,
                                      int32_t y,
                                      uint8_t red,
                                      uint8_t green,
                                      uint8_t blue)
{
  MVM_FramebufferBackend_t *backend = (MVM_FramebufferBackend_t *)user;
  size_t pixelIndex;
  uint16_t pixel;

  if (!backend || !backend->ctx || x < backend->clipX0 || x >= backend->clipX1 ||
      y < backend->clipY0 || y >= backend->clipY1)
  {
    return;
  }

  pixelIndex = (size_t)y * backend->ctx->driver_framebuffer_width + (size_t)x;
  pixel = MVM_lRgb565(red, green, blue);
  if (backend->ctx->driver_framebuffer[pixelIndex] == pixel)
  {
    return;
  }

  backend->ctx->driver_framebuffer[pixelIndex] = pixel;
  if (!backend->hasDirty)
  {
    backend->dirtyX0 = x;
    backend->dirtyY0 = y;
    backend->dirtyX1 = x;
    backend->dirtyY1 = y;
    backend->hasDirty = 1U;
  }
  else
  {
    if (x < backend->dirtyX0)
    {
      backend->dirtyX0 = x;
    }
    if (y < backend->dirtyY0)
    {
      backend->dirtyY0 = y;
    }
    if (x > backend->dirtyX1)
    {
      backend->dirtyX1 = x;
    }
    if (y > backend->dirtyY1)
    {
      backend->dirtyY1 = y;
    }
  }
} /* End of MVM_lFramebufferDrawPoint */

static void MVM_lFramebufferDrawLine(void *user,
                                     int32_t x0,
                                     int32_t y0,
                                     int32_t x1,
                                     int32_t y1,
                                     uint8_t red,
                                     uint8_t green,
                                     uint8_t blue)
{
  int32_t deltaX = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
  int32_t deltaY = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
  int32_t stepX = (x0 < x1) ? 1 : -1;
  int32_t stepY = (y0 < y1) ? 1 : -1;
  int32_t error = deltaX - deltaY;
  int32_t doubledError;

  for (;;)
  {
    MVM_lFramebufferDrawPoint(user, x0, y0, red, green, blue);
    if (x0 == x1 && y0 == y1)
    {
      break;
    }

    doubledError = error * 2;
    if (doubledError > -deltaY)
    {
      error -= deltaY;
      x0 += stepX;
    }
    if (doubledError < deltaX)
    {
      error += deltaX;
      y0 += stepY;
    }
  } /* End of loop */
} /* End of MVM_lFramebufferDrawLine */

static void MVM_lFramebufferFillRect(void *user,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height,
                                     uint8_t red,
                                     uint8_t green,
                                     uint8_t blue)
{
  int32_t drawX;
  int32_t drawY;

  if (width <= 0 || height <= 0)
  {
    return;
  }

  for (drawY = y; drawY < y + height; ++drawY)
  {
    for (drawX = x; drawX < x + width; ++drawX)
    {
      MVM_lFramebufferDrawPoint(user, drawX, drawY, red, green, blue);
    } /* End of loop */
  } /* End of loop */
} /* End of MVM_lFramebufferFillRect */

static uint16_t MVM_lRgb565(uint8_t red, uint8_t green, uint8_t blue)
{
  return (uint16_t)((((uint16_t)red >> 3U) << 11U) |
                    (((uint16_t)green >> 2U) << 5U) |
                    ((uint16_t)blue >> 3U));
} /* End of MVM_lRgb565 */

static void MVM_lDecodeGuestColor(uint32_t color, uint8_t *red, uint8_t *green, uint8_t *blue)
{
  if ((color & 0x80000000U) != 0U || color > 0xFFU)
  {
    *red = (uint8_t)((((color & 0xFFFFU) >> 10U) & 0x1FU) * 255U / 31U);
    *green = (uint8_t)((((color & 0xFFFFU) >> 5U) & 0x1FU) * 255U / 31U);
    *blue = (uint8_t)(((color & 0xFFFFU) & 0x1FU) * 255U / 31U);
  }
  else
  {
    *red = (uint8_t)(((color >> 5U) & 0x07U) * 255U / 7U);
    *green = (uint8_t)(((color >> 2U) & 0x07U) * 255U / 7U);
    *blue = (uint8_t)((color & 0x03U) * 255U / 3U);
  }
} /* End of MVM_lDecodeGuestColor */
