/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeStrings.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Runtime handlers for guest string length and copy operations.
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
 *  Name: MVM_bRuntimeHandleStrings
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_bRuntimeHandleStrings(VMGPContext *ctx, const char *name)
{
  uint32_t p = 0;
  uint32_t dst = 0;
  uint32_t src = 0;
  size_t max_copy = 0;
  size_t n = 0;
  bool bHandled = false;

  if (strcmp(name, "vStrLen") == 0)
  {
    p = ctx->regs[VM_REG_P0];
    ctx->regs[VM_REG_R0] = (p < ctx->mem_size) ? MVM_Lu32RuntimeStrLen(ctx->mem + p, ctx->mem_size - p) : 0u;
    bHandled = true;
  }

  else if (strcmp(name, "vStrCpy") == 0)
  {
    dst = ctx->regs[VM_REG_P0];
    src = ctx->regs[VM_REG_P1];

    if (dst < ctx->mem_size && src < ctx->mem_size)
    {
      max_copy = ctx->mem_size - dst;
      n = MVM_Lu32RuntimeStrLen(ctx->mem + src, ctx->mem_size - src);

      if (n + 1 > max_copy)
      {
        n = max_copy ? max_copy - 1 : 0;
      }
      MVM_vidMemoryWriteWatch(ctx, dst, (uint32_t)(n + 1), "vStrCpy");
      memmove(ctx->mem + dst, ctx->mem + src, n);

      if (max_copy)
      {
        ctx->mem[dst + n] = 0;
      }
    }

    ctx->regs[VM_REG_R0] = dst;
    bHandled = true;
  }

  return bHandled;
} /* End of MVM_bRuntimeHandleStrings */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeStrings.c
 *********************************************************************************************************************/
