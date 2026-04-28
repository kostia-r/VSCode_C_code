#include "MVM.h"
#include "MVM_Device.h"
#include "MVM_Trace.h"
#include "MVM_VmgpDebug.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_STEPS_DEFAULT              (10000000U)
#define MAX_LOGGED_CALLS_DEFAULT       (5000U)
#define VM_STEPS_PER_HOST_TICK         (1000U)

/**
 * @brief Stores the minimal SDL host backend state used by the example runner.
 */
typedef struct SdlBackend
{
  SDL_Window *window;
  SDL_Renderer *renderer;
  uint32_t width;
  uint32_t height;
} SdlBackend;

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
static void present_sdl_backend(SdlBackend *backend)
{
  if (!backend || !backend->renderer)
  {
    return;
  }

  SDL_SetRenderDrawColor(backend->renderer, 0u, 0u, 0u, 255u);
  SDL_RenderClear(backend->renderer);
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
  MVM_RetCode_t retVal;
  SDL_Event event;
  uint32_t host_budget;
  uint32_t step_budget;
  int quit_requested;

  MVM_DumpVmgpSummary(vm);
  MVM_DumpVmgpImports(vm, 64);
  retVal = MVM_SetWdgLimit(vm, 0u);
  if (MVM_OK != retVal)
  {
    return 0;
  }

  present_sdl_backend(backend);

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
    }

    host_budget = VM_STEPS_PER_HOST_TICK;
    while (host_budget != 0u && pump_vm_once(vm, &step_budget) == 0)
    {
      --host_budget;
    }

    present_sdl_backend(backend);

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
