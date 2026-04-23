#include "mophun_vm_internal.h"

#include <string.h>

bool mophun_runtime_handle_heap(VMGPContext *ctx, const char *name)
{
  if (strcmp(name, "vNewPtr") == 0)
  {
    uint32_t size = ctx->regs[VM_REG_P0];
    uint32_t addr = vm_align4(ctx->heap_cur);
    if (size == 0)
      size = 4;
    if (addr + size > ctx->heap_limit)
      ctx->regs[VM_REG_R0] = 0;
    else
    {
      ctx->regs[VM_REG_R0] = addr;
      ctx->heap_cur = addr + size;
    }
    return true;
  }

  if (strcmp(name, "vDisposePtr") == 0 || strcmp(name, "vMemFree") == 0)
  {
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  return false;
}
