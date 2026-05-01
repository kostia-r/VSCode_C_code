/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Trace.c
 *           Module:  MVM_Debug
 *           Target:  Portable C
 *      Description:  Trace execution driver and memory write watch debug hooks.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include <stdio.h>

#include "MVM_Internal.h"

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Runs traced VM execution through the local trace helper.
 */
static bool MVM_lRunTrace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_WatchMemoryWrite
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles VM memory diagnostics.
 *********************************************************************************************************************/
void MVM_WatchMemoryWrite(const VMGPContext *ctx, uint32_t addr, uint32_t size, const char *tag)
{
  (void)ctx;
  (void)addr;
  (void)size;
  (void)tag;
} /* End of MVM_WatchMemoryWrite */

/**********************************************************************************************************************
 *  Name: MVM_RunTrace
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Runs traced VM execution.
 *********************************************************************************************************************/
bool MVM_RunTrace(MpnVM_t *vm, uint32_t max_steps, uint32_t max_logged_calls)
{
  return MVM_lRunTrace(vm, max_steps, max_logged_calls);
} /* End of MVM_RunTrace */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_lRunTrace
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Runs traced VM execution.
 *********************************************************************************************************************/
static bool MVM_lRunTrace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls)
{
  bool bResult = false;
  uint32_t executed = 0;

  if (!ctx || !ctx->mem)
  {
    return false;
  }

  MVM_LOG_T(ctx, "trace-start", "=== execution trace (first %u MVM/system calls) ===\n", max_logged_calls);

  while (ctx->steps < max_steps && ctx->logged_calls < max_logged_calls && !ctx->halted)
  {
    executed = MVM_RunStepsRaw(ctx, 1u);

    if (executed == 0u)
    {
      break;
    }
  } /* End of loop */

  MVM_LOG_T(ctx, "trace-stop", "=== stop ===\n");
  MVM_LOG_T(ctx,
                "trace-stop",
                "steps=%u pc=0x%08X logged_calls=%u heap_cur=0x%08X r0=0x%08X\n",
                ctx->steps,
                ctx->pc,
                ctx->logged_calls,
                ctx->heap_cur,
                ctx->regs[VM_REG_R0]);
  MVM_LOG_T(ctx, "trace-stop", "state=%u error=%u\n", (uint32_t)ctx->state, (uint32_t)ctx->last_error);
  bResult = true;

  return bResult;
} /* End of MVM_lRunTrace */

/**********************************************************************************************************************
 *  END OF FILE MVM_Trace.c
 *********************************************************************************************************************/
