#include "SdlBackend.h"

#include "MVM_Render.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

struct SdlBackend
{
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *framebuffer;
  uint32_t width;
  uint32_t height;
  uint32_t raw_button_state;
  uint32_t pulse_button_state;
  uint32_t next_repeat_ms[SDL_BACKEND_BUTTON_COUNT];
  uint32_t last_frame_serial;
  uint32_t last_clear_serial;
  uint32_t last_draw_command_count;
};

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

static void sdl_render_set_clip(void *user, int enabled, int32_t x, int32_t y, int32_t width, int32_t height)
{
  SdlBackend *backend;
  SDL_Rect clip_rect;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  if (enabled)
  {
    clip_rect.x = (int)x;
    clip_rect.y = (int)y;
    clip_rect.w = (int)width;
    clip_rect.h = (int)height;
    SDL_RenderSetClipRect(backend->renderer, &clip_rect);
  }
  else
  {
    SDL_RenderSetClipRect(backend->renderer, NULL);
  }
}

static void sdl_render_draw_point(void *user, int32_t x, int32_t y, uint8_t red, uint8_t green, uint8_t blue)
{
  SdlBackend *backend;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  SDL_SetRenderDrawColor(backend->renderer, red, green, blue, 255u);
  SDL_RenderDrawPoint(backend->renderer, x, y);
}

static void sdl_render_draw_line(void *user,
                                 int32_t x0,
                                 int32_t y0,
                                 int32_t x1,
                                 int32_t y1,
                                 uint8_t red,
                                 uint8_t green,
                                 uint8_t blue)
{
  SdlBackend *backend;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  SDL_SetRenderDrawColor(backend->renderer, red, green, blue, 255u);
  SDL_RenderDrawLine(backend->renderer, x0, y0, x1, y1);
}

static void sdl_render_fill_rect(void *user,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 uint8_t red,
                                 uint8_t green,
                                 uint8_t blue)
{
  SdlBackend *backend;
  SDL_Rect rect;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  rect.x = (int)x;
  rect.y = (int)y;
  rect.w = (int)width;
  rect.h = (int)height;
  SDL_SetRenderDrawColor(backend->renderer, red, green, blue, 255u);
  SDL_RenderFillRect(backend->renderer, &rect);
}

static uint32_t sdl_platform_get_ticks_ms(void *user)
{
  (void)user;

  return (uint32_t)SDL_GetTicks();
}

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

static void update_backend_pointer_state(SdlBackend *backend, const SDL_MouseButtonEvent *button_event)
{
  if (!backend || !button_event)
  {
    return;
  }

  (void)backend;
  (void)button_event;
}

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

SdlBackend *SdlBackend_Create(const MpnDevProfile_t *profile)
{
  SdlBackend *backend;
  uint32_t width;
  uint32_t height;

  backend = (SdlBackend *)calloc(1u, sizeof(*backend));
  if (!backend)
  {
    return NULL;
  }

  backend->last_clear_serial = 0xFFFFFFFFu;
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
    free(backend);

    return NULL;
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
    SdlBackend_Destroy(backend);

    return NULL;
  }

  backend->renderer = SDL_CreateRenderer(backend->window,
                                         -1,
                                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  if (!backend->renderer)
  {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SdlBackend_Destroy(backend);

    return NULL;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  SDL_RenderSetLogicalSize(backend->renderer, (int)width, (int)height);

  backend->framebuffer = SDL_CreateTexture(backend->renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           (int)width,
                                           (int)height);
  if (!backend->framebuffer)
  {
    fprintf(stderr, "SDL_CreateTexture framebuffer failed: %s\n", SDL_GetError());
    SdlBackend_Destroy(backend);

    return NULL;
  }

  SDL_SetTextureBlendMode(backend->framebuffer, SDL_BLENDMODE_NONE);
  backend->width = width;
  backend->height = height;

  return backend;
}

void SdlBackend_Destroy(SdlBackend *backend)
{
  if (!backend)
  {
    return;
  }

  if (backend->framebuffer)
  {
    SDL_DestroyTexture(backend->framebuffer);
    backend->framebuffer = NULL;
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

  SDL_Quit();
  free(backend);
}

void SdlBackend_AttachVmTiming(MpnVM_t *vm, SdlBackend *backend)
{
  if (!vm || !backend)
  {
    return;
  }

  (void)MVM_SetTickProvider(vm, backend, sdl_platform_get_ticks_ms);
}

void SdlBackend_SyncInputToVm(MpnVM_t *vm, SdlBackend *backend)
{
  if (!vm || !backend)
  {
    return;
  }

  (void)MVM_SetButtonState(vm, backend->raw_button_state | backend->pulse_button_state);
  backend->pulse_button_state = 0u;
}

int SdlBackend_PumpEvents(MpnVM_t *vm, SdlBackend *backend)
{
  SDL_Event event;
  int quit_requested;

  quit_requested = 0;
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
  SdlBackend_SyncInputToVm(vm, backend);

  return quit_requested;
}

void SdlBackend_Present(MpnVM_t *vm, SdlBackend *backend)
{
  MVM_RenderFrameInfo_t frame_info;
  MVM_RenderBackend_t render_backend;
  uint32_t color;
  uint32_t first_command;

  if (!backend || !backend->renderer || !backend->framebuffer || !vm)
  {
    return;
  }

  if (!MVM_RenderGetFrameInfo(vm, &frame_info))
  {
    return;
  }

  color = frame_info.clear_color;
  first_command = backend->last_draw_command_count;

  if (frame_info.clear_serial != backend->last_clear_serial ||
      frame_info.draw_command_count < backend->last_draw_command_count)
  {
    backend->last_clear_serial = frame_info.clear_serial;
    backend->last_draw_command_count = 0u;
    first_command = 0u;

    SDL_SetRenderTarget(backend->renderer, backend->framebuffer);
    SDL_RenderSetClipRect(backend->renderer, NULL);
    set_renderer_guest_color(backend->renderer, color);
    SDL_RenderClear(backend->renderer);
  }

  SDL_SetRenderTarget(backend->renderer, backend->framebuffer);
  render_backend.user = backend;
  render_backend.set_clip = sdl_render_set_clip;
  render_backend.draw_point = sdl_render_draw_point;
  render_backend.draw_line = sdl_render_draw_line;
  render_backend.fill_rect = sdl_render_fill_rect;
  (void)MVM_RenderReplayCommands(vm, &render_backend, first_command);

  SDL_RenderSetClipRect(backend->renderer, NULL);
  if (MVM_RenderGetFrameInfo(vm, &frame_info))
  {
    backend->last_draw_command_count = frame_info.draw_command_count;
  }

  if (frame_info.frame_serial != backend->last_frame_serial)
  {
    backend->last_frame_serial = frame_info.frame_serial;
  }

  SDL_SetRenderTarget(backend->renderer, NULL);
  SDL_RenderSetClipRect(backend->renderer, NULL);
  SDL_SetRenderDrawColor(backend->renderer, 0u, 0u, 0u, 255u);
  SDL_RenderClear(backend->renderer);
  SDL_RenderCopy(backend->renderer, backend->framebuffer, NULL, NULL);
  SDL_RenderPresent(backend->renderer);
}

void SdlBackend_Delay(uint32_t delay_ms)
{
  SDL_Delay(delay_ms);
}
