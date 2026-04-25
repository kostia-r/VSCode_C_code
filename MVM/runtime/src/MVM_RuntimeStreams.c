/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeStreams.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Runtime handlers for guest stream open, seek, read, and close operations.
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
 * @brief Finds an active VM resource stream by handle.
 */
static VMGPStream *find_stream(VMGPContext *ctx, uint32_t handle);

/**
 * @brief Allocates a free VM resource stream slot.
 */
static VMGPStream *alloc_stream(VMGPContext *ctx);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_bRuntimeHandleStream
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_HandleRuntimeStream(VMGPContext *ctx, const char *name)
{
  uint32_t mode = 0;
  uint32_t resid = 0;
  VMGPStream *s = NULL;
  const VMGPResource *res = NULL;
  int32_t where = 0;
  uint32_t whence = 0;
  int32_t pos = -1;
  uint32_t buf = 0;
  uint32_t count = 0;
  uint32_t avail = 0;
  bool bHandled = false;

  if (strcmp(name, "vStreamOpen") == 0)
  {
    mode = ctx->regs[VM_REG_P1];
    resid = mode >> 16;
    s = alloc_stream(ctx);

    if (!s)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    if (resid != 0)
    {
      res = MVM_GetVmgpResource(ctx, resid);

      if (!res)
      {
        memset(s, 0, sizeof(*s));
        ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

        return true;
      }

      s->base = ctx->res_offset + res->offset;
      s->size = res->size;
      s->resource_id = resid;
    }
    else
    {
      s->base = ctx->res_offset;
      s->size = ctx->header.res_size;
    }

    s->pos = 0;
    ctx->regs[VM_REG_R0] = s->handle;
    bHandled = true;
  }

  else if (strcmp(name, "vStreamSeek") == 0)
  {
    s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    where = vm_reg_s32(ctx->regs[VM_REG_P1]);
    whence = ctx->regs[VM_REG_P2];
    pos = -1;

    if (!s)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    if (whence == 0)
    {
      pos = where;
    }
    else if (whence == 1)
    {
      pos = (int32_t)s->pos + where;
    }
    else if (whence == 2)
    {
      pos = (int32_t)s->size + where;
    }

    if (pos < 0)
    {
      pos = 0;
    }

    if ((uint32_t)pos > s->size)
    {
      pos = (int32_t)s->size;
    }

    s->pos = (uint32_t)pos;
    ctx->regs[VM_REG_R0] = s->pos;
    bHandled = true;
  }

  else if (strcmp(name, "vStreamRead") == 0)
  {
    s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    buf = ctx->regs[VM_REG_P1];
    count = ctx->regs[VM_REG_P2];

    if (!s || buf >= ctx->mem_size)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    avail = (s->pos < s->size) ? (s->size - s->pos) : 0u;

    if (count > avail)
    {
      count = avail;
    }

    if ((size_t)buf + count > ctx->mem_size)
    {
      count = (uint32_t)(ctx->mem_size - buf);
    }

    if ((size_t)s->base + s->pos + count > ctx->mem_size)
    {
      count = 0;
    }

    MVM_WatchMemoryWrite(ctx, buf, count, "vStreamRead");
    memcpy(ctx->mem + buf, ctx->mem + s->base + s->pos, count);
    s->pos += count;
    ctx->regs[VM_REG_R0] = count;
    bHandled = true;
  }

  else if (strcmp(name, "vStreamClose") == 0)
  {
    s = find_stream(ctx, ctx->regs[VM_REG_P0]);

    if (s)
    {
      memset(s, 0, sizeof(*s));
    }
    ctx->regs[VM_REG_R0] = 0;
    bHandled = true;
  }

  return bHandled;
} /* End of MVM_bRuntimeHandleStream */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: find_stream
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static VMGPStream *find_stream(VMGPContext *ctx, uint32_t handle)
{
  uint32_t i;
  VMGPStream *stream = NULL;

  for (i = 0; i < VMGP_MAX_STREAMS; ++i)
  {
    if (ctx->streams[i].used && ctx->streams[i].handle == handle)
    {
      stream = &ctx->streams[i];
      break;
    }
  } /* End of loop */

  return stream;
} /* End of find_stream */

/**********************************************************************************************************************
 *  Name: alloc_stream
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static VMGPStream *alloc_stream(VMGPContext *ctx)
{
  uint32_t i;
  VMGPStream *stream = NULL;

  for (i = 0; i < VMGP_MAX_STREAMS; ++i)
  {
    if (!ctx->streams[i].used)
    {
      memset(&ctx->streams[i], 0, sizeof(ctx->streams[i]));
      ctx->streams[i].used = true;
      ctx->streams[i].handle = i;
      stream = &ctx->streams[i];
      break;
    }
  } /* End of loop */

  return stream;
} /* End of alloc_stream */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeStreams.c
 *********************************************************************************************************************/
