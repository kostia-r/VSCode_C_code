#include "mophun_vm_internal.h"

#include <string.h>

bool mophun_runtime_handle_caps(VMGPContext *ctx, const char *name)
{
  uint32_t query;
  uint32_t out;

  if (strcmp(name, "vGetCaps") != 0)
  {
    return false;
  }

  query = ctx->regs[VM_REG_P0];
  out = ctx->regs[VM_REG_P1];

  if (query == 0 && mophun_runtime_mem_range_ok(ctx, out, 8))
  {
    vm_write_u16_le(ctx->mem + out + 0, 8);
    vm_write_u16_le(ctx->mem + out + 2, 8);
    vm_write_u16_le(ctx->mem + out + 4, 101);
    vm_write_u16_le(ctx->mem + out + 6, 80);
    ctx->regs[VM_REG_R0] = 1;
    return true;
  }

  if (query == 2 && mophun_runtime_mem_range_ok(ctx, out, 4))
  {
    vm_write_u16_le(ctx->mem + out + 0, 4);
    vm_write_u16_le(ctx->mem + out + 2, 0x000F);
    ctx->regs[VM_REG_R0] = 1;
    return true;
  }

  if (query == 3 && mophun_runtime_mem_range_ok(ctx, out, 4))
  {
    vm_write_u16_le(ctx->mem + out + 0, 4);
    vm_write_u16_le(ctx->mem + out + 2, 0x00A7);
    ctx->regs[VM_REG_R0] = 1;
    return true;
  }

  if (query == 4 && mophun_runtime_mem_range_ok(ctx, out, 12))
  {
    vm_write_u16_le(ctx->mem + out + 0, 12);
    vm_write_u16_le(ctx->mem + out + 2, 0x25);
    vm_write_u32_le(ctx->mem + out + 4, (1u << 16) | 3u);
    vm_write_u32_le(ctx->mem + out + 8, 0);
    ctx->regs[VM_REG_R0] = 1;
    return true;
  }

  ctx->regs[VM_REG_R0] = 0;
  return true;
}
