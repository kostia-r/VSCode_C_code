#include "mophun_vm_internal.h"

#include "mophun_syscalls.h"

#include <string.h>

static bool mophun_runtime_try_host_syscall(VMGPContext *ctx, const char *name)
{
  uint32_t i;

  if (!ctx || !name || !ctx->syscalls)
  {
    return false;
  }

  for (i = 0; i < ctx->syscall_count; ++i)
  {
    const MophunSyscall *syscall = &ctx->syscalls[i];
    if (syscall->name && syscall->fn && strcmp(syscall->name, name) == 0)
    {
      ctx->regs[VM_REG_R0] = syscall->fn(ctx, syscall->user);
      return true;
    }
  }

  return false;
}

bool vmgp_handle_import_call(VMGPContext *ctx, uint32_t pool_index)
{
  const char *name = vmgp_get_import_name(ctx, pool_index);

  if (mophun_runtime_try_host_syscall(ctx, name) ||
      mophun_runtime_handle_stream(ctx, name) ||
      mophun_runtime_handle_caps(ctx, name) ||
      mophun_runtime_handle_decompress(ctx, name) ||
      mophun_runtime_handle_heap(ctx, name) ||
      mophun_runtime_handle_time_random(ctx, name) ||
      mophun_runtime_handle_strings(ctx, name) ||
      mophun_runtime_handle_misc(ctx, name))
  {
    return true;
  }

  ctx->regs[VM_REG_R0] = 0;
  return true;
}
