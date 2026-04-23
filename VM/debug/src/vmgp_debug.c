#include "mophun_vm_internal.h"

void vmgp_dump_summary(const VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  mophun_vm_logf(ctx, "=== VMGP summary ===\n");
  mophun_vm_logf(ctx, "magic             : %.4s\n", ctx->header.magic);
  mophun_vm_logf(ctx, "unknown1          : 0x%04X\n", ctx->header.unknown1);
  mophun_vm_logf(ctx, "unknown2          : 0x%04X\n", ctx->header.unknown2);
  mophun_vm_logf(ctx, "stack_words       : %u (0x%X)\n", ctx->header.stack_words, ctx->header.stack_words);
  mophun_vm_logf(ctx, "code_size         : %u (0x%X)\n", ctx->header.code_size, ctx->header.code_size);
  mophun_vm_logf(ctx, "data_size         : %u (0x%X)\n", ctx->header.data_size, ctx->header.data_size);
  mophun_vm_logf(ctx, "bss_size          : %u (0x%X)\n", ctx->header.bss_size, ctx->header.bss_size);
  mophun_vm_logf(ctx, "res_size          : %u (0x%X)\n", ctx->header.res_size, ctx->header.res_size);
  mophun_vm_logf(ctx, "pool_slots        : %u\n", ctx->header.pool_slots);
  mophun_vm_logf(ctx, "string_table_size : %u (0x%X)\n", ctx->header.string_size, ctx->header.string_size);
  mophun_vm_logf(ctx, "code_offset(vm)   : 0x%X\n", ctx->code_offset);
  mophun_vm_logf(ctx, "code_offset(file) : 0x%X\n", ctx->code_file_offset);
  mophun_vm_logf(ctx, "data_offset       : 0x%X\n", ctx->data_offset);
  mophun_vm_logf(ctx, "bss_offset        : 0x%X\n", ctx->bss_offset);
  mophun_vm_logf(ctx, "res_offset        : 0x%X\n", ctx->res_offset);
  mophun_vm_logf(ctx, "pool_offset(file) : 0x%X\n", ctx->pool_offset);
  mophun_vm_logf(ctx, "strtab_offset     : 0x%X\n", ctx->strtab_offset);
  mophun_vm_logf(ctx, "vm_end            : 0x%X\n", ctx->vm_end);
  mophun_vm_logf(ctx, "heap_base         : 0x%X\n", ctx->heap_base);
  mophun_vm_logf(ctx, "heap_limit        : 0x%X\n", ctx->heap_limit);
  mophun_vm_logf(ctx, "stack_top         : 0x%X\n", ctx->stack_top);
  mophun_vm_logf(ctx, "resource_count    : %u\n", ctx->resource_count);
}

void vmgp_dump_imports(const VMGPContext *ctx, uint32_t max_count)
{
  uint32_t i;

  if (!ctx || !ctx->pool)
  {
    return;
  }

  mophun_vm_logf(ctx, "=== imports (leading type=0x02 pool entries) ===\n");
  for (i = 1; i <= ctx->header.pool_slots && i <= max_count; ++i)
  {
    const VMGPPoolEntry *e = vmgp_get_pool_entry(ctx, i);
    if (!e || e->type != 0x02)
    {
      break;
    }
    mophun_vm_logf(ctx, "[%03u] %s\n", i, vmgp_get_import_name(ctx, i));
  }
}
