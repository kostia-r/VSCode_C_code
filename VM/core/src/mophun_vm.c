#include "mophun_vm_internal.h"

#include "mophun_config.h"
#include "mophun_syscalls.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t mophun_vm_storage_size(void)
{
  return sizeof(MophunVM);
}

size_t mophun_vm_storage_align(void)
{
  typedef struct MophunVMAlignProbe
  {
    char c;
    MophunVM vm;
  } MophunVMAlignProbe;

  return offsetof(MophunVMAlignProbe, vm);
}

MophunVM *mophun_vm_from_storage(void *storage, size_t storage_size)
{
  size_t align = mophun_vm_storage_align();

  if (!storage || storage_size < sizeof(MophunVM))
  {
    return NULL;
  }
  if (align != 0u && ((uintptr_t)storage % align) != 0u)
  {
    return NULL;
  }
  return (MophunVM *)storage;
}

bool vmgp_init(VMGPContext *ctx, const uint8_t *data, size_t size)
{
  return vmgp_init_with_platform(ctx, data, size, NULL);
}

bool vmgp_init_with_platform(VMGPContext *ctx,
                             const uint8_t *data,
                             size_t size,
                             const MophunPlatform *platform)
{
  if (!ctx || !data || size < sizeof(VMGPHeader))
  {
    return false;
  }
  memset(ctx, 0, sizeof(*ctx));
  if (platform)
  {
    ctx->platform = *platform;
  }
  ctx->data = data;
  ctx->size = size;
  ctx->next_stream_handle = 0x30u;
  ctx->random_state = 1u;
  return true;
}

bool mophun_vm_init(MophunVM *vm, const uint8_t *image, size_t image_size)
{
  return mophun_vm_init_with_platform(vm, image, image_size, NULL);
}

bool mophun_vm_init_with_platform(MophunVM *vm,
                                  const uint8_t *image,
                                  size_t image_size,
                                  const MophunPlatform *platform)
{
  if (!vmgp_init_with_platform(vm, image, image_size, platform))
  {
    return false;
  }
  if (!vmgp_parse_header(vm) || !vmgp_load_pool(vm))
  {
    vmgp_free(vm);
    return false;
  }
  return true;
}

void *mophun_vm_calloc(VMGPContext *ctx, size_t count, size_t size)
{
  if (ctx && ctx->platform.calloc)
  {
    return ctx->platform.calloc(ctx->platform.user, count, size);
  }
#if MOPHUN_VM_ENABLE_DEFAULT_ALLOCATOR
  return calloc(count, size);
#else
  (void)ctx;
  (void)count;
  (void)size;
  return NULL;
#endif
}

void mophun_vm_free_mem(VMGPContext *ctx, void *ptr)
{
  if (!ptr)
  {
    return;
  }
  if (ctx && ctx->platform.free)
  {
    ctx->platform.free(ctx->platform.user, ptr);
    return;
  }
#if MOPHUN_VM_ENABLE_DEFAULT_ALLOCATOR
  free(ptr);
#else
  (void)ctx;
#endif
}

void mophun_vm_logf(const VMGPContext *ctx, const char *fmt, ...)
{
  char buffer[MOPHUN_VM_LOG_BUFFER_SIZE];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);

  if (ctx && ctx->platform.log)
  {
    ctx->platform.log(ctx->platform.user, buffer);
    return;
  }

#if MOPHUN_VM_ENABLE_DEFAULT_LOGGER
  fputs(buffer, stdout);
#else
  (void)ctx;
#endif
}

void vmgp_free(VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }
  mophun_vm_free_mem(ctx, ctx->pool);
  mophun_vm_free_mem(ctx, ctx->resources);
  mophun_vm_free_mem(ctx, ctx->mem);
  memset(ctx, 0, sizeof(*ctx));
}

void mophun_vm_free(MophunVM *vm)
{
  vmgp_free(vm);
}

void mophun_vm_set_syscalls(MophunVM *vm, const MophunSyscall *syscalls, uint32_t count)
{
  if (!vm)
  {
    return;
  }
  vm->syscalls = syscalls;
  vm->syscall_count = count;
}
