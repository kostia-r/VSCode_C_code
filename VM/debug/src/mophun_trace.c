#include "mophun_vm_internal.h"

void mophun_vm_memory_write_watch(const VMGPContext *ctx, uint32_t addr, uint32_t size, const char *tag)
{
  (void)ctx;
  (void)addr;
  (void)size;
  (void)tag;
}

bool vmgp_run_trace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls)
{
  if (!ctx || !ctx->mem)
  {
    return false;
  }

  mophun_vm_logf(ctx, "=== execution trace (first %u VM/system calls) ===\n", max_logged_calls);
  while (ctx->steps < max_steps && ctx->logged_calls < max_logged_calls && !ctx->halted)
  {
    if (!vmgp_step(ctx))
    {
      break;
    }
  }
  mophun_vm_logf(ctx, "=== stop ===\n");
  mophun_vm_logf(ctx,
                 "steps=%u pc=0x%08X logged_calls=%u heap_cur=0x%08X r0=0x%08X\n",
                 ctx->steps,
                 ctx->pc,
                 ctx->logged_calls,
                 ctx->heap_cur,
                 ctx->regs[VM_REG_R0]);
  return true;
}

bool mophun_vm_run_trace(MophunVM *vm, uint32_t max_steps, uint32_t max_logged_calls)
{
  return vmgp_run_trace(vm, max_steps, max_logged_calls);
}
