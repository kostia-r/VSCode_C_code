#include "MVM.h"
#include "MVM_Device.h"
#include "InputScript.h"
#include "SdlBackend.h"
#include "VmRunner.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STEPS_DEFAULT              (100000000U)
#define MAX_LOGGED_CALLS_DEFAULT       (500000U)

/**
 * @brief Describes parsed command-line options for the VM runner.
 */
typedef struct AppOptions
{
  const char *image_path;
  const char *profile_name;
  const char *input_script_path;
  const char *record_dir;
  uint32_t max_steps;
  uint32_t max_logged_calls;
  uint32_t duration_ms;
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

  provider->file = fopen(path, "rb+");
  if (!provider->file)
  {
    provider->file = fopen(path, "rb");
  }

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
          "Usage: %s <decrypted.mpn> [profile_name] [max_steps] [max_logged_calls] "
          "[--duration-ms N] [--input-script PATH] [--record-dir DIR]\n",
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
  int numeric_count;

  if (argc < 2)
  {
    print_usage(argv[0]);

    return 0;
  }

  options->image_path = argv[1];
  options->profile_name = NULL;
  options->input_script_path = NULL;
  options->record_dir = NULL;
  options->max_steps = MAX_STEPS_DEFAULT;
  options->max_logged_calls = MAX_LOGGED_CALLS_DEFAULT;
  options->duration_ms = 0u;

  arg_index = 2;
  numeric_count = 0;
  while (arg_index < argc)
  {
    const char *arg;

    arg = argv[arg_index];
    if (strcmp(arg, "--duration-ms") == 0)
    {
      if (++arg_index >= argc || !is_numeric_arg(argv[arg_index]))
      {
        print_usage(argv[0]);
        return 0;
      }
      options->duration_ms = (uint32_t)strtoul(argv[arg_index], NULL, 0);
    }
    else if (strcmp(arg, "--input-script") == 0)
    {
      if (++arg_index >= argc)
      {
        print_usage(argv[0]);
        return 0;
      }
      options->input_script_path = argv[arg_index];
    }
    else if (strcmp(arg, "--record-dir") == 0)
    {
      if (++arg_index >= argc)
      {
        print_usage(argv[0]);
        return 0;
      }
      options->record_dir = argv[arg_index];
    }
    else if (arg[0] == '-' && arg[1] == '-')
    {
      fprintf(stderr, "Unknown option: %s\n", arg);
      print_usage(argv[0]);
      return 0;
    }
    else
    {
      if (!options->profile_name && numeric_count == 0 && !is_numeric_arg(arg))
      {
        options->profile_name = arg;
      }
      else if (numeric_count == 0 && is_numeric_arg(arg))
      {
        options->max_steps = (uint32_t)strtoul(arg, NULL, 0);
        ++numeric_count;
      }
      else if (numeric_count == 1 && is_numeric_arg(arg))
      {
        options->max_logged_calls = (uint32_t)strtoul(arg, NULL, 0);
        ++numeric_count;
      }
      else
      {
        print_usage(argv[0]);
        return 0;
      }
    }
    ++arg_index;
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

static uint32_t update_scripted_input(void *user, uint32_t elapsed_ms)
{
  return InputScript_GetButtonMask((InputScript *)user, elapsed_ms);
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
  SdlBackend *backend;
  InputScript *input_script;
  VmRunnerOptions runner_options;
  MVM_MemReqs_t memory_requirements;
  const MpnDevProfile_t *profile;
  MVM_RetCode_t retVal;
  int exit_code;

  file_provider = (FileImageSource){0};
  image_source = (MpnImageSource_t){0};
  vm_storage = NULL;
  vm = NULL;
  backend = NULL;
  input_script = NULL;
  runner_options = (VmRunnerOptions){0};
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

  backend = SdlBackend_Create(profile);
  if (!backend)
  {
    return exit_code;
  }

  if (options.input_script_path)
  {
    input_script = InputScript_Load(options.input_script_path);
    if (!input_script || InputScript_GetLastError(input_script)[0] != '\0')
    {
      fprintf(stderr,
              "Could not load input script: %s (%s)\n",
              options.input_script_path,
              InputScript_GetLastError(input_script));
      InputScript_Destroy(input_script);
      SdlBackend_Destroy(backend);

      return exit_code;
    }
  }

  if (options.record_dir && !SdlBackend_StartRecording(backend, options.record_dir))
  {
    fprintf(stderr, "Could not start recording in: %s\n", options.record_dir);
    InputScript_Destroy(input_script);
    SdlBackend_Destroy(backend);

    return exit_code;
  }

  /* This sample integration opens the VMGP image through a file-backed image
   * source descriptor. The actual read callbacks are compiled into Config/,
   * so the runner only chooses which image instance to execute.
   */
  if (!open_image_source(options.image_path, &file_provider))
  {
    fprintf(stderr, "Could not load file.\n");
    SdlBackend_StopRecording(backend);
    InputScript_Destroy(input_script);
    SdlBackend_Destroy(backend);

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
    SdlBackend_StopRecording(backend);
    InputScript_Destroy(input_script);
    SdlBackend_Destroy(backend);
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
    SdlBackend_StopRecording(backend);
    InputScript_Destroy(input_script);
    SdlBackend_Destroy(backend);

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
    SdlBackend_StopRecording(backend);
    InputScript_Destroy(input_script);
    SdlBackend_Destroy(backend);

    return exit_code;
  }

  /* Drive the VM through the non-blocking step API until one of the local
   * runner limits is reached.
   */
  runner_options.max_steps = options.max_steps;
  runner_options.max_logged_calls = options.max_logged_calls;
  runner_options.duration_ms = options.duration_ms;
  runner_options.input = input_script ? update_scripted_input : NULL;
  runner_options.input_user = input_script;
  VmRunner_Run(vm, backend, &runner_options);
  print_stop_summary(vm);

  exit_code = 0;
  MVM_Free(vm);
  free(vm_storage);
  close_image_source(&file_provider);
  SdlBackend_StopRecording(backend);
  InputScript_Destroy(input_script);
  SdlBackend_Destroy(backend);

  return exit_code;
}
