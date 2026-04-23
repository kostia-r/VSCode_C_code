/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeCommon.c
 *           Module:  MVM_Runtime
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
 *  Name: MVM_LbRuntimeMemRangeOk
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_LbRuntimeMemRangeOk(const VMGPContext *ctx, uint32_t addr, uint32_t size)
{
  return ctx && addr <= ctx->mem_size && size <= ctx->mem_size - addr;
} /* End of MVM_LbRuntimeMemRangeOk */

/**********************************************************************************************************************
 *  Name: MVM_Lu32RuntimeStrLen
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
uint32_t MVM_Lu32RuntimeStrLen(const uint8_t *s, size_t max_len)
{
  uint32_t n = 0;

  while (n < max_len && s[n] != 0)
  {
    ++n;
  } /* End of loop */

  return n;
} /* End of MVM_Lu32RuntimeStrLen */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeCommon.c
 *********************************************************************************************************************/
