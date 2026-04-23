#include "mophun_vm_internal.h"

#include <string.h>

bool mophun_runtime_handle_time_random(VMGPContext *ctx, const char *name)
{
  if (strcmp(name, "vGetTickCount") == 0)
  {
    if (ctx->platform.get_ticks_ms)
    {
      ctx->tick_count = ctx->platform.get_ticks_ms(ctx->platform.user);
    }
    else
    {
      ctx->tick_count += 16u;
    }
    ctx->regs[VM_REG_R0] = ctx->tick_count;
    return true;
  }

  if (strcmp(name, "vSetRandom") == 0)
  {
    ctx->random_state = ctx->regs[VM_REG_P0] ? ctx->regs[VM_REG_P0] : 1u;
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  if (strcmp(name, "vGetRandom") == 0)
  {
    if (ctx->platform.get_random)
    {
      ctx->regs[VM_REG_R0] = ctx->platform.get_random(ctx->platform.user);
    }
    else
    {
      ctx->random_state = ctx->random_state * 1103515245u + 12345u;
      ctx->regs[VM_REG_R0] = (ctx->random_state >> 16) & 0xFFFFu;
    }
    return true;
  }

  return false;
}
