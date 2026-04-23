/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Trace.c
 *           Module:  MVM_Debug
 *           Target:  Portable C
 *      Description:  Mophun VM component source.
 *            Notes:  Structured according to project styling guidelines.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_vidMemoryWriteWatch
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles VM memory diagnostics.
 *********************************************************************************************************************/
void MVM_vidMemoryWriteWatch(const VMGPContext *ctx, uint32_t addr, uint32_t size, const char *tag)
{
  (void)ctx;
  (void)addr;
  (void)size;
  (void)tag;
} /* End of MVM_vidMemoryWriteWatch */

/**********************************************************************************************************************
 *  Name: MVM_LbRunTrace
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Runs traced VM execution.
 *********************************************************************************************************************/
bool MVM_LbRunTrace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls)
{
  if (!ctx || !ctx->mem)
  {
    return false;
  }

  MVM_LvidLogf(ctx, "=== execution trace (first %u MVM/system calls) ===\n", max_logged_calls);

  while (ctx->steps < max_steps && ctx->logged_calls < max_logged_calls && !ctx->halted)
  {

    if (!MVM_LbPipStep(ctx))
    {
      break;
    }
  } /* End of loop */

  MVM_LvidLogf(ctx, "=== stop ===\n");
  MVM_LvidLogf(ctx,
  "steps=%u pc=0x%08X logged_calls=%u heap_cur=0x%08X r0=0x%08X\n",
  ctx->steps,
  ctx->pc,
  ctx->logged_calls,
  ctx->heap_cur,
  ctx->regs[VM_REG_R0]);
  return true;
} /* End of MVM_LbRunTrace */

/**********************************************************************************************************************
 *  Name: MVM_bRunTrace
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Runs traced VM execution.
 *********************************************************************************************************************/
bool MVM_bRunTrace(MophunVM *vm, uint32_t max_steps, uint32_t max_logged_calls)
{
  return MVM_LbRunTrace(vm, max_steps, max_logged_calls);
} /* End of MVM_bRunTrace */

/**********************************************************************************************************************
 *  END OF FILE MVM_Trace.c
 *********************************************************************************************************************/
