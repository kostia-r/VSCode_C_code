/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeCommon.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Shared runtime utility helpers for VM memory ranges and string handling.
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
 *  Name: MVM_RuntimeMemRangeOk
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_RuntimeMemRangeOk(const VMGPContext *ctx, uint32_t addr, uint32_t size)
{
  return MVM_GuestTryMapConst(ctx, addr, size) != NULL;
} /* End of MVM_RuntimeMemRangeOk */

uint8_t *MVM_GuestTryMap(VMGPContext *ctx, uint32_t addr, uint32_t size)
{
  size_t offset;

  if (!ctx)
  {
    return NULL;
  }

  if ((size_t)addr <= ctx->mem_low_size && (size_t)size <= ctx->mem_low_size - (size_t)addr)
  {
    return ctx->mem + addr;
  }

  if (addr < ctx->mem_high_base)
  {
    return NULL;
  }

  offset = (size_t)(addr - ctx->mem_high_base);
  if (offset <= ctx->mem_high_size && (size_t)size <= ctx->mem_high_size - offset)
  {
    return ctx->mem_high + offset;
  }

  return NULL;
} /* End of MVM_GuestTryMap */

const uint8_t *MVM_GuestTryMapConst(const VMGPContext *ctx, uint32_t addr, uint32_t size)
{
  return MVM_GuestTryMap((VMGPContext *)ctx, addr, size);
} /* End of MVM_GuestTryMapConst */

uint32_t MVM_GuestContiguousSize(const VMGPContext *ctx, uint32_t addr)
{
  size_t remaining;

  if (!ctx)
  {
    return 0u;
  }
  if ((size_t)addr < ctx->mem_low_size)
  {
    remaining = ctx->mem_low_size - (size_t)addr;
  }
  else if (addr >= ctx->mem_high_base && (size_t)(addr - ctx->mem_high_base) < ctx->mem_high_size)
  {
    remaining = ctx->mem_high_size - (size_t)(addr - ctx->mem_high_base);
  }
  else
  {
    return 0u;
  }

  return (remaining > UINT32_MAX) ? UINT32_MAX : (uint32_t)remaining;
} /* End of MVM_GuestContiguousSize */

uint8_t *MVM_GuestMap(VMGPContext *ctx, uint32_t addr, uint32_t size, const char *operation)
{
  uint8_t *memory;

  memory = MVM_GuestTryMap(ctx, addr, size);
  if (!memory && ctx)
  {
    MVM_LOG_E(ctx,
              "mem-oob",
              "%s guest range OOB: pc=0x%08X addr=0x%08X size=%u\n",
              operation ? operation : "memory",
              ctx->pc,
              addr,
              size);
    MVM_EmitEvent(ctx, MVM_EVENT_MEMORY_OOB, addr, size);
    MVM_SetErrorRaw(ctx, MVM_E_EXECUTION);
    if (size <= sizeof(ctx->guest_fault_scratch))
    {
      memory = ctx->guest_fault_scratch;
    }
  }

  return memory;
} /* End of MVM_GuestMap */

const uint8_t *MVM_GuestMapConst(VMGPContext *ctx, uint32_t addr, uint32_t size, const char *operation)
{
  return MVM_GuestMap(ctx, addr, size, operation);
} /* End of MVM_GuestMapConst */

bool MVM_GuestCopy(VMGPContext *ctx, uint32_t dst, uint32_t src, uint32_t size)
{
  uint8_t *destination;
  const uint8_t *source;

  destination = MVM_GuestTryMap(ctx, dst, size);
  source = MVM_GuestTryMapConst(ctx, src, size);
  if (!destination || !source)
  {
    (void)MVM_GuestMap(ctx, !destination ? dst : src, size, "copy");
    return false;
  }

  memmove(destination, source, size);
  return true;
} /* End of MVM_GuestCopy */

bool MVM_GuestSet(VMGPContext *ctx, uint32_t dst, uint8_t value, uint32_t size)
{
  uint8_t *destination;

  destination = MVM_GuestTryMap(ctx, dst, size);
  if (!destination)
  {
    (void)MVM_GuestMap(ctx, dst, size, "set");
    return false;
  }

  memset(destination, value, size);
  return true;
} /* End of MVM_GuestSet */

/**********************************************************************************************************************
 *  Name: MVM_RuntimeStrLen
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
uint32_t MVM_RuntimeStrLen(const uint8_t *s, size_t max_len)
{
  uint32_t n = 0;

  while (n < max_len && s[n] != 0)
  {
    ++n;
  } /* End of loop */

  return n;
} /* End of MVM_RuntimeStrLen */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeCommon.c
 *********************************************************************************************************************/
