#include "mophun_vm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *vmgp_pool_type_name(uint8_t type)
{
  switch (type)
  {
  case 0x00:
    return "null";
  case 0x02:
    return "import";
  case 0x11:
    return "code";
  case 0x13:
    return "export";
  case 0x18:
    return "bytes";
  case 0x21:
    return "u32";
  case 0x23:
    return "const?";
  case 0x24:
    return "str?";
  case 0x25:
    return "res?";
  case 0x26:
    return "ptr?";
  case 0x41:
    return "addr?";
  case 0x67:
    return "sys?";
  default:
    return "unknown";
  }
}

size_t vmgp_pool_size_bytes(const VMGPHeader *header)
{
  return header ? (size_t)header->pool_slots * VMGP_POOL_SLOT_SIZE : 0u;
}

const VMGPPoolEntry *vmgp_get_pool_entry(const VMGPContext *ctx, uint32_t pool_index_1based)
{
  if (!ctx || !ctx->pool || pool_index_1based == 0 || pool_index_1based > ctx->header.pool_slots)
  {
    return NULL;
  }
  return &ctx->pool[pool_index_1based - 1u];
}

uint32_t vmgp_resolve_pool_value(const VMGPContext *ctx, const VMGPPoolEntry *entry)
{
  if (!ctx || !entry)
  {
    return 0;
  }

  switch (entry->type)
  {
  case 0x21: /* .data */
  case 0x23: /* global .data */
    return ctx->data_offset + entry->value;
  case 0x41: /* .bss */
    return ctx->bss_offset + entry->value;
  case 0x11: /* .text */
  case 0x67: /* absolute const */
  default:
    return entry->value;
  }
}

static bool vmgp_load_resources(VMGPContext *ctx)
{
  uint32_t prev = 0;
  uint32_t count = 0;
  uint32_t i;

  if (!ctx || ctx->header.res_size < 8)
  {
    return true;
  }

  for (i = 0; i + 4 <= ctx->header.res_size; i += 4)
  {
    uint32_t off = vm_read_u32_le(ctx->data + ctx->res_file_offset + i);
    if (off == 0)
    {
      break;
    }
    if (off >= ctx->header.res_size || off < prev)
    {
      break;
    }
    prev = off;
    count++;
  }

  if (count == 0)
  {
    return true;
  }

  ctx->resources = (VMGPResource *)mophun_vm_calloc(ctx, count, sizeof(VMGPResource));
  if (!ctx->resources)
  {
    return false;
  }
  ctx->resource_count = count;

  for (i = 0; i < count; ++i)
  {
    uint32_t off = vm_read_u32_le(ctx->data + ctx->res_file_offset + i * 4u);
    uint32_t next = (i + 1u < count)
                        ? vm_read_u32_le(ctx->data + ctx->res_file_offset + (i + 1u) * 4u)
                        : ctx->header.res_size;
    ctx->resources[i].id = i + 1u;
    ctx->resources[i].offset = off;
    ctx->resources[i].size = (next > off) ? (next - off) : 0u;
  }
  return true;
}

static bool vmgp_build_vm_memory(VMGPContext *ctx)
{
  ctx->vm_end = ctx->res_offset + ctx->header.res_size;
  ctx->heap_base = vm_align4(ctx->vm_end);
  ctx->heap_cur = ctx->heap_base;
  ctx->heap_limit = ctx->heap_base + VM_HEAP_EXTRA;
  ctx->stack_top = ctx->heap_limit + VM_STACK_EXTRA;
  ctx->mem_size = ctx->stack_top + 0x100u;

  ctx->mem = (uint8_t *)mophun_vm_calloc(ctx, ctx->mem_size, 1);
  if (!ctx->mem)
  {
    return false;
  }

  memcpy(ctx->mem + ctx->data_offset, ctx->data + ctx->data_file_offset, ctx->header.data_size);
  if (ctx->header.res_size > 0)
  {
    memcpy(ctx->mem + ctx->res_offset, ctx->data + ctx->res_file_offset, ctx->header.res_size);
  }

  ctx->pc = 0;
  ctx->regs[VM_REG_SP] = ctx->stack_top;
  ctx->regs[VM_REG_ZERO] = 0;
  return true;
}

static const char *vm_file_str(const VMGPContext *ctx, uint32_t off)
{
  if (!ctx || ctx->strtab_offset + off >= ctx->size)
  {
    return NULL;
  }
  return (const char *)(ctx->data + ctx->strtab_offset + off);
}

const char *vmgp_get_import_name(const VMGPContext *ctx, uint32_t pool_index_1based)
{
  static char bad[32];
  const VMGPPoolEntry *entry = vmgp_get_pool_entry(ctx, pool_index_1based);
  const char *name;

  if (!entry)
  {
    snprintf(bad, sizeof(bad), "<bad:%u>", pool_index_1based);
    return bad;
  }
  if (entry->type != 0x02)
  {
    snprintf(bad, sizeof(bad), "<type:%02X>", entry->type);
    return bad;
  }

  name = vm_file_str(ctx, entry->aux24);
  return name ? name : "<str-oob>";
}

const VMGPResource *vmgp_get_resource(const VMGPContext *ctx, uint32_t resource_id)
{
  uint32_t i;

  if (!ctx || !ctx->resources || resource_id == 0)
  {
    return NULL;
  }

  for (i = 0; i < ctx->resource_count; ++i)
  {
    if (ctx->resources[i].id == resource_id)
    {
      return &ctx->resources[i];
    }
  }
  return NULL;
}

bool vmgp_parse_header(VMGPContext *ctx)
{
  if (!ctx || !ctx->data || ctx->size < sizeof(VMGPHeader))
  {
    return false;
  }

  memcpy(ctx->header.magic, ctx->data + 0x00, 4);
  ctx->header.unknown1 = vm_read_u16_le(ctx->data + 0x04);
  ctx->header.unknown2 = vm_read_u16_le(ctx->data + 0x06);
  ctx->header.stack_words = vm_read_u16_le(ctx->data + 0x08);
  ctx->header.unknown3 = ctx->data[0x0A];
  ctx->header.unknown4 = ctx->data[0x0B];
  ctx->header.code_size = vm_read_u32_le(ctx->data + 0x0C);
  ctx->header.data_size = vm_read_u32_le(ctx->data + 0x10);
  ctx->header.bss_size = vm_read_u32_le(ctx->data + 0x14);
  ctx->header.res_size = vm_read_u32_le(ctx->data + 0x18);
  ctx->header.unknown5 = vm_read_u32_le(ctx->data + 0x1C);
  ctx->header.pool_slots = vm_read_u32_le(ctx->data + 0x20);
  ctx->header.string_size = vm_read_u32_le(ctx->data + 0x24);

  ctx->header_valid = (memcmp(ctx->header.magic, VMGP_MAGIC, 4) == 0);
  if (!ctx->header_valid)
  {
    return false;
  }

  ctx->code_file_offset = (uint32_t)sizeof(VMGPHeader);
  ctx->data_file_offset = ctx->code_file_offset + ctx->header.code_size;
  ctx->code_offset = 0;
  ctx->data_offset = 0;
  ctx->bss_offset = ctx->data_offset + ctx->header.data_size;
  ctx->res_offset = ctx->bss_offset + ctx->header.bss_size;
  ctx->res_file_offset = ctx->data_file_offset + ctx->header.data_size;
  ctx->pool_offset = ctx->res_file_offset + ctx->header.res_size;
  ctx->strtab_offset = ctx->pool_offset + ctx->header.pool_slots * VMGP_POOL_SLOT_SIZE;

  if ((size_t)ctx->strtab_offset + ctx->header.string_size > ctx->size)
  {
    return false;
  }

  return true;
}

bool vmgp_load_pool(VMGPContext *ctx)
{
  uint32_t i;

  if (!ctx || !ctx->header_valid)
  {
    return false;
  }

  ctx->pool = (VMGPPoolEntry *)mophun_vm_calloc(ctx, ctx->header.pool_slots, sizeof(VMGPPoolEntry));
  if (!ctx->pool)
  {
    return false;
  }

  for (i = 0; i < ctx->header.pool_slots; ++i)
  {
    uint32_t off = ctx->pool_offset + i * VMGP_POOL_SLOT_SIZE;
    ctx->pool[i].type = ctx->data[off + 0];
    ctx->pool[i].aux24 = (uint32_t)ctx->data[off + 1] |
                         ((uint32_t)ctx->data[off + 2] << 8) |
                         ((uint32_t)ctx->data[off + 3] << 16);
    ctx->pool[i].value = vm_read_u32_le(ctx->data + off + 4);
  }

  if (!vmgp_load_resources(ctx))
  {
    return false;
  }
  if (!vmgp_build_vm_memory(ctx))
  {
    return false;
  }
  return true;
}
