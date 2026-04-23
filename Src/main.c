#include "mophun_vm.h"
#include "mophun_trace.h"
#include "mophun_vmgp_debug.h"

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

static void *host_calloc(void *user, size_t count, size_t size)
{
  (void)user;
  return calloc(count, size);
}

static void host_free(void *user, void *ptr)
{
  (void)user;
  free(ptr);
}

static int host_log(void *user, const char *message)
{
  (void)user;
  return fputs(message, stdout);
}

int main(int argc, char **argv)
{
  size_t file_size = 0;
  uint8_t *file_data;
  void *vm_storage;
  MophunVM *vm;
  MophunPlatform platform = {0};
  uint32_t max_steps = 5000000;
  uint32_t max_logged_calls = 500;

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

  vm_storage = malloc(mophun_vm_storage_size());
  vm = mophun_vm_from_storage(vm_storage, mophun_vm_storage_size());
  if (!vm)
  {
    fprintf(stderr, "Could not allocate VM storage.\n");
    free(file_data);
    free(vm_storage);
    return 1;
  }

  platform.calloc = host_calloc;
  platform.free = host_free;
  platform.log = host_log;

  if (!mophun_vm_init_with_platform(vm, file_data, file_size, &platform))
  {
    fprintf(stderr, "Failed to initialize VMGP context.\n");
    free(file_data);
    free(vm_storage);
    return 1;
  }

  vmgp_dump_summary(vm);
  vmgp_dump_imports(vm, 64);
  mophun_vm_run_trace(vm, max_steps, max_logged_calls);

  mophun_vm_free(vm);
  free(vm_storage);
  free(file_data);
  return 0;
}
