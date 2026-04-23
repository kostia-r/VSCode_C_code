#include "mophun_vm_internal.h"

#include <string.h>

static VMGPStream *find_stream(VMGPContext *ctx, uint32_t handle)
{
  uint32_t i;
  for (i = 0; i < VMGP_MAX_STREAMS; ++i)
  {
    if (ctx->streams[i].used && ctx->streams[i].handle == handle)
    {
      return &ctx->streams[i];
    }
  }
  return NULL;
}

static VMGPStream *alloc_stream(VMGPContext *ctx)
{
  uint32_t i;
  for (i = 0; i < VMGP_MAX_STREAMS; ++i)
  {
    if (!ctx->streams[i].used)
    {
      memset(&ctx->streams[i], 0, sizeof(ctx->streams[i]));
      ctx->streams[i].used = true;
      ctx->streams[i].handle = i;
      return &ctx->streams[i];
    }
  }
  return NULL;
}

bool mophun_runtime_handle_stream(VMGPContext *ctx, const char *name)
{
  if (strcmp(name, "vStreamOpen") == 0)
  {
    uint32_t mode = ctx->regs[VM_REG_P1];
    uint32_t resid = mode >> 16;
    VMGPStream *s = alloc_stream(ctx);
    if (!s)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    if (resid != 0)
    {
      const VMGPResource *res = vmgp_get_resource(ctx, resid);
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
    return true;
  }

  if (strcmp(name, "vStreamSeek") == 0)
  {
    VMGPStream *s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    int32_t where = vm_reg_s32(ctx->regs[VM_REG_P1]);
    uint32_t whence = ctx->regs[VM_REG_P2];
    int32_t pos = -1;
    if (!s)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    if (whence == 0)
      pos = where;
    else if (whence == 1)
      pos = (int32_t)s->pos + where;
    else if (whence == 2)
      pos = (int32_t)s->size + where;
    if (pos < 0)
      pos = 0;
    if ((uint32_t)pos > s->size)
      pos = (int32_t)s->size;
    s->pos = (uint32_t)pos;
    ctx->regs[VM_REG_R0] = s->pos;
    return true;
  }

  if (strcmp(name, "vStreamRead") == 0)
  {
    VMGPStream *s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    uint32_t buf = ctx->regs[VM_REG_P1];
    uint32_t count = ctx->regs[VM_REG_P2];
    uint32_t avail;
    if (!s || buf >= ctx->mem_size)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    avail = (s->pos < s->size) ? (s->size - s->pos) : 0u;
    if (count > avail)
      count = avail;
    if ((size_t)buf + count > ctx->mem_size)
      count = (uint32_t)(ctx->mem_size - buf);
    if ((size_t)s->base + s->pos + count > ctx->mem_size)
      count = 0;
    mophun_vm_memory_write_watch(ctx, buf, count, "vStreamRead");
    memcpy(ctx->mem + buf, ctx->mem + s->base + s->pos, count);
    s->pos += count;
    ctx->regs[VM_REG_R0] = count;
    return true;
  }

  if (strcmp(name, "vStreamClose") == 0)
  {
    VMGPStream *s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    if (s)
      memset(s, 0, sizeof(*s));
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  return false;
}
