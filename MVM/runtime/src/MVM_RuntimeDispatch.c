/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeDispatch.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Runtime import dispatcher for host syscalls and built-in service groups.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include <string.h>

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Dispatches an import to a host-provided syscall.
 */
static bool MVM_lTryHostSyscall(VMGPContext *ctx, const char *name);

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

  bHandled = MVM_lTryHostSyscall(ctx, name) ||
             MVM_HandleImport(ctx, name);

  if (!bHandled)
  {
    MVM_LOG_W(ctx, "missing-syscall", "unhandled import pool[%u] name=%s\n", pool_index, name ? name : "<null>");
    MVM_EmitEvent(ctx, MVM_EVENT_MISSING_SYSCALL, pool_index, 0u);
    ctx->regs[VM_REG_R0] = 0;
  }

  return true;
} /* End of MVM_HandleRuntimeImportCall */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_lTryHostSyscall
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
static bool MVM_lTryHostSyscall(VMGPContext *ctx, const char *name)
{
  uint32_t i;
  const MpnSyscall_t *syscall = NULL;
  bool bHandled = false;

  if (!ctx || !name || !ctx->syscalls)
  {
    return false;
  }

  for (i = 0; i < ctx->syscall_count; ++i)
  {
    syscall = &ctx->syscalls[i];

    if (syscall->name && syscall->fn && strcmp(syscall->name, name) == 0)
    {
      ctx->regs[VM_REG_R0] = syscall->fn(ctx, syscall->user);
      bHandled = true;
      break;
    }
  } /* End of loop */

  return bHandled;
} /* End of MVM_lTryHostSyscall */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeDispatch.c
 *********************************************************************************************************************/
