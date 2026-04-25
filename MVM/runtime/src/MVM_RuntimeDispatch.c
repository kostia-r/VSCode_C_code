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
static bool MVM_LbRuntimeTryHostSyscall(VMGPContext *ctx, const char *name);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_bRuntimeHandleImportCall
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_bRuntimeHandleImportCall(VMGPContext *ctx, uint32_t pool_index)
{
  const char *name = MVM_pudtVmgpGetImportName(ctx, pool_index);
  bool bHandled = false;

  bHandled = MVM_LbRuntimeTryHostSyscall(ctx, name) ||
             MVM_bRuntimeHandleStream(ctx, name) ||
             MVM_bRuntimeHandleDecompress(ctx, name) ||
             MVM_bRuntimeHandleHeap(ctx, name) ||
             MVM_bRuntimeHandleStrings(ctx, name);

  if (!bHandled)
  {
    ctx->regs[VM_REG_R0] = 0;
  }

  return true;
} /* End of MVM_bRuntimeHandleImportCall */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_LbRuntimeTryHostSyscall
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
static bool MVM_LbRuntimeTryHostSyscall(VMGPContext *ctx, const char *name)
{
  uint32_t i;
  const MophunSyscall *syscall = NULL;
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
} /* End of MVM_LbRuntimeTryHostSyscall */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeDispatch.c
 *********************************************************************************************************************/
