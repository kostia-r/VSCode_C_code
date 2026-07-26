/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  OpenMophun
 *             File:  MVM_RuntimeDispatch.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Runtime import dispatcher for VM-owned service groups.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_HandleRuntimeImportCall
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_HandleRuntimeImportCall(VMGPContext *ctx, uint32_t pool_index)
{
  const char *name = MVM_GetVmgpImportName(ctx, pool_index);
  bool bHandled = false;

  MVM_EmitEvent(ctx, MVM_EVENT_IMPORT_CALL, pool_index, 0u);

  bHandled = MVM_HandleImport(ctx, name);

  if (!bHandled)
  {
    MVM_LOG_W(ctx, "missing-syscall", "unhandled import pool[%u] name=%s\n", pool_index, name ? name : "<null>");
    MVM_EmitEvent(ctx, MVM_EVENT_MISSING_SYSCALL, pool_index, 0u);
    ctx->regs[VM_REG_R0] = 0;
  }

  if (ctx->drivers.display_flush && !MVM_RenderApplyPendingFramebuffer(ctx))
  {
    MVM_LOG_W(ctx, "display-render", "Unable to apply pending framebuffer commands\n");
  }

  return true;
} /* End of MVM_HandleRuntimeImportCall */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeDispatch.c
 *********************************************************************************************************************/
