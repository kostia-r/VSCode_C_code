#include "MVM_Vm.h"
#include "MVM_Cfg.h"
#include "MVM_Trace.h"
#include "MVM_VmgpDebug.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *load_file(const char *path, size_t *out_size)
{
  FILE *f = fopen(path, "rb");
  long sz;
  uint8_t *buf;
  size_t read_sz;

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

static int pump_vm_once(MophunVM *vm, uint32_t *step_budget)
{
  int status = 0;

  if (MVM_tenuGetState(vm) == MVM_TENU_STATE_READY || MVM_tenuGetState(vm) == MVM_TENU_STATE_RUNNING)
  {
    if (*step_budget == 0u)
    {
      status = 1;
    }
    else
    {
      if (!MVM_bStep(vm))
      {
        status = 1;
      }
      else
      {
        --(*step_budget);
      }
    }
  }
  else
  {
    status = 1;
  }

  return status;
}

static int is_numeric_arg(const char *value)
{
  const unsigned char *p = (const unsigned char *)value;

  if (!value || !*value)
  {
    return 0;
  }

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

int main(int argc, char **argv)
{
  size_t file_size = 0;
  uint8_t *file_data;
  void *vm_storage;
  MophunVM *vm;
  MVM_tstConfig stConfig = MVM_kstConfig;
  MVM_tstMemoryRequirements memory_requirements = {0};
  const MophunDeviceProfile *pstSelectedProfile = NULL;
  const char *profile_name = NULL;
  uint32_t max_steps = 5000000;
  uint32_t max_logged_calls = 1000;
  uint32_t step_budget = 0;
  uint32_t i = 0u;
  int arg_index = 0;
  MVM_tenuState state;
  MVM_tenuError error;

  if (argc < 2 || argc > 5)
  {
    fprintf(stderr, "Usage: %s <decrypted.mpn> [profile_name] [max_steps] [max_logged_calls]\n", argv[0]);
    return 1;
  }

  arg_index = 2;
  if (argc > arg_index && !is_numeric_arg(argv[arg_index]))
  {
    profile_name = argv[arg_index];
    pstSelectedProfile = MVM_Cfg_pcdtFindDeviceProfileByName(&stConfig, profile_name);
    if (!pstSelectedProfile)
    {
      fprintf(stderr, "Unknown device profile: %s\n", profile_name);
      fprintf(stderr, "Available profiles:");

      for (i = 0u; i < stConfig.device_profile_count; ++i)
      {
        if (stConfig.device_profiles[i].name)
        {
          fprintf(stderr, " %s", stConfig.device_profiles[i].name);
        }
      }
      fprintf(stderr, "\n");

      return 1;
    }

    stConfig.device_profile = pstSelectedProfile;
    ++arg_index;
  }

  if (argc > arg_index)
  {
    max_steps = (uint32_t)strtoul(argv[arg_index], NULL, 0);
    ++arg_index;
  }
  if (argc > arg_index)
  {
    max_logged_calls = (uint32_t)strtoul(argv[arg_index], NULL, 0);
  }

  file_data = load_file(argv[1], &file_size);
  if (!file_data)
  {
    fprintf(stderr, "Could not load file.\n");
    return 1;
  }

  vm_storage = malloc(MVM_udtGetStorageSize());
  vm = MVM_pudtGetVmFromStorage(vm_storage, MVM_udtGetStorageSize());
  if (!vm)
  {
    fprintf(stderr, "Could not allocate VM storage.\n");
    free(file_data);
    free(vm_storage);
    return 1;
  }

  if (!MVM_bQueryMemoryRequirements(file_data, file_size, &memory_requirements))
  {
    fprintf(stderr, "Could not query VM memory requirements.\n");
    free(file_data);
    free(vm_storage);

    return 1;
  }

  if (!MVM_bInitWithConfig(vm, file_data, file_size, &stConfig))
  {
    fprintf(stderr,
            "Failed to initialize VMGP context. required_pool=%llu configured_pool=%llu error=%u\n",
            (unsigned long long)memory_requirements.runtime_pool_bytes,
            (unsigned long long)stConfig.runtime_pool_size,
            (unsigned)MVM_tenuGetLastError(vm));
    free(file_data);
    free(vm_storage);

    return 1;
  }

  MVM_vidVmgpDumpSummary(vm);
  MVM_vidVmgpDumpImports(vm, 64);
  MVM_vidSetWatchdogLimit(vm, 0u);
  step_budget = max_steps;

  while (pump_vm_once(vm, &step_budget) == 0)
  {
    if (MVM_u32GetLoggedCalls(vm) >= max_logged_calls)
    {
      break;
    }
  }

  state = MVM_tenuGetState(vm);
  error = MVM_tenuGetLastError(vm);
  fprintf(stdout,
          "=== stop ===\nsteps=%u pc=0x%08X logged_calls=%u state=%u error=%u\n",
          MVM_u32GetExecutedSteps(vm),
          MVM_u32GetProgramCounter(vm),
          MVM_u32GetLoggedCalls(vm),
          (unsigned)state,
          (unsigned)error);

  MVM_vidFree(vm);
  free(vm_storage);
  free(file_data);
  return 0;
}
