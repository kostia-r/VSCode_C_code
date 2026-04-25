#include "MVM_Vm.h"
#include "MVM_Trace.h"
#include "MVM_VmgpDebug.h"

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

static int host_log(void *user, const char *message)
{
  (void)user;
  return fputs(message, stdout);
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

int main(int argc, char **argv)
{
  size_t file_size = 0;
  uint8_t *file_data;
  void *vm_storage;
  void *guest_memory = NULL;
  void *pool_entries = NULL;
  void *resource_entries = NULL;
  MophunVM *vm;
  MophunPlatform platform = {0};
  MVM_tstMemoryRequirements memory_requirements = {0};
  MVM_tstMemoryConfig memory_config = {0};
  uint32_t max_steps = 5000000;
  uint32_t max_logged_calls = 1000;
  uint32_t step_budget = 0;
  MVM_tenuState state;
  MVM_tenuError error;

  if (argc < 2 || argc > 4)
  {
    fprintf(stderr, "Usage: %s <decrypted.mpn> [max_steps] [max_logged_calls]\n", argv[0]);
    return 1;
  }

  if (argc >= 3)
  {
    max_steps = (uint32_t)strtoul(argv[2], NULL, 0);
  }
  if (argc >= 4)
  {
    max_logged_calls = (uint32_t)strtoul(argv[3], NULL, 0);
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

  guest_memory = calloc(1u, memory_requirements.guest_memory_bytes);
  pool_entries = calloc(1u, memory_requirements.pool_entries_bytes);
  resource_entries = memory_requirements.resource_entries_bytes != 0u
                   ? calloc(1u, memory_requirements.resource_entries_bytes)
                   : NULL;

  if (!guest_memory ||
      !pool_entries ||
      (memory_requirements.resource_entries_bytes != 0u && !resource_entries))
  {
    fprintf(stderr, "Could not allocate static VM memory buffers.\n");
    free(resource_entries);
    free(pool_entries);
    free(guest_memory);
    free(file_data);
    free(vm_storage);

    return 1;
  }

  memory_config.guest_memory = guest_memory;
  memory_config.guest_memory_size = memory_requirements.guest_memory_bytes;
  memory_config.pool_entries = pool_entries;
  memory_config.pool_entries_size = memory_requirements.pool_entries_bytes;
  memory_config.resource_entries = resource_entries;
  memory_config.resource_entries_size = memory_requirements.resource_entries_bytes;

  platform.log = host_log;

  if (!MVM_bInitWithPlatformAndMemory(vm, file_data, file_size, &platform, &memory_config))
  {
    fprintf(stderr, "Failed to initialize VMGP context.\n");
    free(resource_entries);
    free(pool_entries);
    free(guest_memory);
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
  free(resource_entries);
  free(pool_entries);
  free(guest_memory);
  free(vm_storage);
  free(file_data);
  return 0;
}
