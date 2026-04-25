/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeHeap.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Runtime heap allocation and free-related import handlers.
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
 *  Name: MVM_bRuntimeHandleHeap
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_HandleRuntimeHeap(VMGPContext *ctx, const char *name)
{
  uint32_t size = 0;
  uint32_t addr = 0;
  bool bHandled = false;

  if (strcmp(name, "vNewPtr") == 0)
  {
    size = ctx->regs[VM_REG_P0];
    addr = vm_align4(ctx->heap_cur);

    if (size == 0)
    {
      size = 4;
    }

    if (addr + size > ctx->heap_limit)
    {
      ctx->regs[VM_REG_R0] = 0;
    }

    else
    {
      ctx->regs[VM_REG_R0] = addr;
      ctx->heap_cur = addr + size;
    }

    bHandled = true;
  }

  else if (strcmp(name, "vDisposePtr") == 0 || strcmp(name, "vMemFree") == 0)
  {
    ctx->regs[VM_REG_R0] = 0;

    bHandled = true;
  }

  return bHandled;
} /* End of MVM_bRuntimeHandleHeap */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeHeap.c
 *********************************************************************************************************************/
