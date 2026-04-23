/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeStrings.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Mophun VM component source.
 *            Notes:  Structured according to project styling guidelines.
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
  if (strcmp(name, "vStrLen") == 0)
  {
    uint32_t p = ctx->regs[VM_REG_P0];
    ctx->regs[VM_REG_R0] = (p < ctx->mem_size) ? MVM_Lu32RuntimeStrLen(ctx->mem + p, ctx->mem_size - p) : 0u;
    return true;
  }

  if (strcmp(name, "vStrCpy") == 0)
  {
    uint32_t dst = ctx->regs[VM_REG_P0];
    uint32_t src = ctx->regs[VM_REG_P1];

    if (dst < ctx->mem_size && src < ctx->mem_size)
    {
      size_t max_copy = ctx->mem_size - dst;
      size_t n = MVM_Lu32RuntimeStrLen(ctx->mem + src, ctx->mem_size - src);

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
    return true;
  }

  return false;
} /* End of MVM_bRuntimeHandleStrings */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeStrings.c
 *********************************************************************************************************************/
