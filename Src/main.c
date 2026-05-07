#include "MVM.h"
#include "MVM_Device.h"
#include "MVM_Render.h"
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
  SDL_Texture *framebuffer;
  uint32_t width;
  uint32_t height;
  uint32_t raw_button_state;
  uint32_t pulse_button_state;
  uint32_t next_repeat_ms[SDL_BACKEND_BUTTON_COUNT];
  uint32_t last_frame_serial;
  uint32_t last_clear_serial;
  uint32_t last_draw_command_count;
} SdlBackend;

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
  backend->framebuffer = NULL;
  backend->width = 0u;
  backend->height = 0u;
  backend->raw_button_state = 0u;
  backend->pulse_button_state = 0u;
  memset(backend->next_repeat_ms, 0, sizeof(backend->next_repeat_ms));
  backend->last_frame_serial = 0u;
  backend->last_clear_serial = 0xFFFFFFFFu;
  backend->last_draw_command_count = 0u;

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
                                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
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

  backend->framebuffer = SDL_CreateTexture(backend->renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           (int)width,
                                           (int)height);
  if (!backend->framebuffer)
  {
    fprintf(stderr, "SDL_CreateTexture framebuffer failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(backend->renderer);
    backend->renderer = NULL;
    SDL_DestroyWindow(backend->window);
    backend->window = NULL;
    SDL_Quit();

    return 0;
  }

  SDL_SetTextureBlendMode(backend->framebuffer, SDL_BLENDMODE_NONE);

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
  if (!vm || !backend)
  {
    return;
  }

  (void)MVM_SetTickProvider(vm, backend, sdl_platform_get_ticks_ms);
}

/**
 * @brief Copies the host button mask into the current VM instance.
 */
static void sync_backend_input_to_vm(MpnVM_t *vm, SdlBackend *backend)
{
  if (!vm || !backend)
  {
    return;
  }

  (void)MVM_SetButtonState(vm, backend->raw_button_state | backend->pulse_button_state);
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

  backend->width = 0u;
  backend->height = 0u;
  SDL_Quit();
}

/**
 * @brief Presents the minimal host window for one frame.
 */
static void present_sdl_backend(MpnVM_t *vm, SdlBackend *backend)
{
  MVM_RenderFrameInfo_t frame_info;
  MVM_RenderBackend_t render_backend;
  uint32_t color;
  uint32_t first_command;

  if (!backend || !backend->renderer || !backend->framebuffer)
  {
    return;
  }

  if (!vm)
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
  MVM_RenderFrameInfo_t frame_info;
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

    frame_serial_before = 0u;
    if (MVM_RenderGetFrameInfo(vm, &frame_info))
    {
      frame_serial_before = frame_info.frame_serial;
    }

    host_budget = VM_MAX_STEPS_PER_HOST_FRAME;
    while (host_budget != 0u && pump_vm_once(vm, &step_budget) == 0)
    {
      --host_budget;

      if (MVM_RenderGetFrameInfo(vm, &frame_info) && frame_info.frame_serial != frame_serial_before)
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
