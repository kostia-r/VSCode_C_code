/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeCaps.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Runtime import handlers that answer device capability queries from the guest.
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
 *  Name: MVM_bRuntimeHandleCaps
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_bRuntimeHandleCaps(VMGPContext *ctx, const char *name)
{
  uint32_t query;
  uint32_t out;
  bool bHandled = false;

  if (strcmp(name, "vGetCaps") != 0)
  {
    return false;
  }

  query = ctx->regs[VM_REG_P0];
  out = ctx->regs[VM_REG_P1];

  if (query == 0 && MVM_LbRuntimeMemRangeOk(ctx, out, 8))
  {
    vm_write_u16_le(ctx->mem + out + 0, 8);
    vm_write_u16_le(ctx->mem + out + 2, 8);
    vm_write_u16_le(ctx->mem + out + 4, 101);
    vm_write_u16_le(ctx->mem + out + 6, 80);
    ctx->regs[VM_REG_R0] = 1;
    bHandled = true;
  }

  else if (query == 2 && MVM_LbRuntimeMemRangeOk(ctx, out, 4))
  {
    vm_write_u16_le(ctx->mem + out + 0, 4);
    vm_write_u16_le(ctx->mem + out + 2, 0x000F);
    ctx->regs[VM_REG_R0] = 1;
    bHandled = true;
  }

  else if (query == 3 && MVM_LbRuntimeMemRangeOk(ctx, out, 4))
  {
    vm_write_u16_le(ctx->mem + out + 0, 4);
    vm_write_u16_le(ctx->mem + out + 2, 0x00A7);
    ctx->regs[VM_REG_R0] = 1;
    bHandled = true;
  }

  else if (query == 4 && MVM_LbRuntimeMemRangeOk(ctx, out, 12))
  {
    vm_write_u16_le(ctx->mem + out + 0, 12);
    vm_write_u16_le(ctx->mem + out + 2, 0x25);
    vm_write_u32_le(ctx->mem + out + 4, (1u << 16) | 3u);
    vm_write_u32_le(ctx->mem + out + 8, 0);
    ctx->regs[VM_REG_R0] = 1;
    bHandled = true;
  }

  if (!bHandled)
  {
    ctx->regs[VM_REG_R0] = 0;
  }

  return true;
} /* End of MVM_bRuntimeHandleCaps */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeCaps.c
 *********************************************************************************************************************/
