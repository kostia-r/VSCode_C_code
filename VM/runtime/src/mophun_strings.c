#include "mophun_vm_internal.h"

#include <string.h>

bool mophun_runtime_handle_strings(VMGPContext *ctx, const char *name)
{
  if (strcmp(name, "vStrLen") == 0)
  {
    uint32_t p = ctx->regs[VM_REG_P0];
    ctx->regs[VM_REG_R0] = (p < ctx->mem_size) ? mophun_runtime_strlen(ctx->mem + p, ctx->mem_size - p) : 0u;
    return true;
  }

  if (strcmp(name, "vStrCpy") == 0)
  {
    uint32_t dst = ctx->regs[VM_REG_P0];
    uint32_t src = ctx->regs[VM_REG_P1];
    if (dst < ctx->mem_size && src < ctx->mem_size)
    {
      size_t max_copy = ctx->mem_size - dst;
      size_t n = mophun_runtime_strlen(ctx->mem + src, ctx->mem_size - src);
      if (n + 1 > max_copy)
        n = max_copy ? max_copy - 1 : 0;
      mophun_vm_memory_write_watch(ctx, dst, (uint32_t)(n + 1), "vStrCpy");
      memmove(ctx->mem + dst, ctx->mem + src, n);
      if (max_copy)
        ctx->mem[dst + n] = 0;
    }
    ctx->regs[VM_REG_R0] = dst;
    return true;
  }

  return false;
}
