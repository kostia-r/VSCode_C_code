#include "MVM.h"
#include "MVM_Device.h"
#include "MVM_Trace.h"
#include "MVM_VmgpDebug.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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
 * @brief Loads one VMGP image into a host buffer.
 */
static uint8_t *load_file(const char *path, size_t *out_size)
{
  FILE *f;
  long sz;
  uint8_t *buf;
  size_t read_sz;

  f = fopen(path, "rb");
  if (!f)
  {
    fprintf(stderr, "Failed to open: %s\n", path);

    return NULL;
  }

  if (fseek(f, 0, SEEK_END) != 0)
  {
    fclose(f);

    return NULL;
  }

  sz = ftell(f);
  if (sz < 0)
  {
    fclose(f);

    return NULL;
  }

  rewind(f);
  buf = (uint8_t *)malloc((size_t)sz);
  if (!buf)
  {
    fclose(f);

    return NULL;
  }

  read_sz = fread(buf, 1, (size_t)sz, f);
  fclose(f);

  if (read_sz != (size_t)sz)
  {
    free(buf);

    return NULL;
  }

  *out_size = (size_t)sz;

  return buf;
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
  const MophunDeviceProfile *profile;

  fprintf(stderr, "Available profiles:");

  profile_count = MVM_u32GetDeviceProfileCount();
  for (i = 0u; i < profile_count; ++i)
  {
    profile = MVM_pcdtGetDeviceProfile(i);
    if (profile && profile->name)
    {
      fprintf(stderr, " %s", profile->name);
    }
  }

  fprintf(stderr, "\n");
}

/**
 * @brief Executes one bounded VM step.
 */
static int pump_vm_once(MophunVM *vm, uint32_t *step_budget)
{
  MVM_tenuReturnCode retVal;
  MVM_tenuState state;

  state = MVM_tenuGetState(vm);
  if (state != MVM_TENU_STATE_READY && state != MVM_TENU_STATE_RUNNING)
  {
    return 1;
  }

  if (*step_budget == 0u)
  {
    return 1;
  }

  retVal = MVM_enuStep(vm);
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
  options->max_steps = 5000000u;
  options->max_logged_calls = 1000u;

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

  if (!MVM_pcdtFindDeviceProfileByName(profile_name))
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
static MophunVM *create_vm(void *storage)
{
  size_t storage_size;
  MophunVM *vm;

  storage_size = MVM_udtGetStorageSize();
  vm = MVM_pudtGetVmFromStorage(storage, storage_size);

  return vm;
}

/**
 * @brief Runs the VM until one local stop condition is reached.
 */
static int run_vm(MophunVM *vm, uint32_t max_steps, uint32_t max_logged_calls)
{
  MVM_tenuReturnCode retVal;
  uint32_t step_budget;

  MVM_vidVmgpDumpSummary(vm);
  MVM_vidVmgpDumpImports(vm, 64);
  retVal = MVM_enuSetWatchdogLimit(vm, 0u);
  if (MVM_OK != retVal)
  {
    return 0;
  }

  step_budget = max_steps;
  while (pump_vm_once(vm, &step_budget) == 0)
  {
    if (MVM_u32GetLoggedCalls(vm) >= max_logged_calls)
    {
      break;
    }
  }

  return 1;
}

/**
 * @brief Prints the final VM execution summary.
 */
static void print_stop_summary(MophunVM *vm)
{
  MVM_tenuState state;
  MVM_tenuError error;

  state = MVM_tenuGetState(vm);
  error = MVM_tenuGetLastError(vm);
  fprintf(stdout,
          "=== stop ===\nsteps=%u pc=0x%08X logged_calls=%u state=%u error=%u\n",
          MVM_u32GetExecutedSteps(vm),
          MVM_u32GetProgramCounter(vm),
          MVM_u32GetLoggedCalls(vm),
          (unsigned)state,
          (unsigned)error);
}

int main(int argc, char **argv)
{
  AppOptions options;
  size_t file_size;
  uint8_t *file_data;
  void *vm_storage;
  MophunVM *vm;
  MVM_tstMemoryRequirements memory_requirements;
  MVM_tenuReturnCode retVal;
  int exit_code;

  file_size = 0u;
  file_data = NULL;
  vm_storage = NULL;
  vm = NULL;
  memory_requirements = (MVM_tstMemoryRequirements){0};
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

  /* This sample integration keeps the VMGP image in a host buffer and passes
   * that buffer to the VM initialization path.
   */
  file_data = load_file(options.image_path, &file_size);
  if (!file_data)
  {
    fprintf(stderr, "Could not load file.\n");

    return exit_code;
  }

  /* The host owns raw VM storage and asks the library to construct a VM
   * instance inside that storage block.
   */
  vm_storage = malloc(MVM_udtGetStorageSize());
  vm = create_vm(vm_storage);
  if (!vm)
  {
    fprintf(stderr, "Could not allocate VM storage.\n");
    free(file_data);
    free(vm_storage);

    return exit_code;
  }

  /* Query image-driven runtime memory needs before init so the integration can
   * validate its configured runtime pool capacity.
   */
  retVal = MVM_enuQueryMemoryRequirements(file_data, file_size, &memory_requirements);
  if (MVM_OK != retVal)
  {
    fprintf(stderr, "Could not query VM memory requirements. ret=%u\n", (unsigned)retVal);
    MVM_vidFree(vm);
    free(vm_storage);
    free(file_data);

    return exit_code;
  }

  /* Initialize the VM through the simple public API. The host only provides
   * VM storage, the VMGP image, and the optional device profile name.
   */
  retVal = MVM_enuInit(vm, file_data, file_size, options.profile_name);
  if (MVM_OK != retVal)
  {
    fprintf(stderr,
            "Failed to initialize VMGP context. ret=%u required_pool=%llu error=%u\n",
            (unsigned)retVal,
            (unsigned long long)memory_requirements.runtime_pool_bytes,
            (unsigned)MVM_tenuGetLastError(vm));
    MVM_vidFree(vm);
    free(vm_storage);
    free(file_data);

    return exit_code;
  }

  /* Drive the VM through the non-blocking step API until one of the local
   * runner limits is reached.
   */
  run_vm(vm, options.max_steps, options.max_logged_calls);
  print_stop_summary(vm);

  exit_code = 0;
  MVM_vidFree(vm);
  free(vm_storage);
  free(file_data);

  return exit_code;
}
