/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeTimeRandom.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Runtime handlers for guest tick-count and random-number services.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include <string.h>

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_bRuntimeHandleTimeRandom
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_bRuntimeHandleTimeRandom(VMGPContext *ctx, const char *name)
{
  bool bHandled = false;

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
    bHandled = true;
  }

  else if (strcmp(name, "vSetRandom") == 0)
  {
    ctx->random_state = ctx->regs[VM_REG_P0] ? ctx->regs[VM_REG_P0] : 1u;
    ctx->regs[VM_REG_R0] = 0;
    bHandled = true;
  }

  else if (strcmp(name, "vGetRandom") == 0)
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

    bHandled = true;
  }

  return bHandled;
} /* End of MVM_bRuntimeHandleTimeRandom */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeTimeRandom.c
 *********************************************************************************************************************/
