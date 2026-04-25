/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_VmgpDebug.c
 *           Module:  MVM_Debug
 *           Target:  Portable C
 *      Description:  VMGP image summary and import-dump debug output helpers.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_DumpVmgpSummary
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
void MVM_DumpVmgpSummary(const VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  MVM_LOG_I(ctx, "vmgp-summary", "=== VMGP summary ===\n");
  MVM_LOG_I(ctx, "vmgp-summary", "magic             : %.4s\n", ctx->header.magic);
  MVM_LOG_I(ctx, "vmgp-summary", "unknown1          : 0x%04X\n", ctx->header.unknown1);
  MVM_LOG_I(ctx, "vmgp-summary", "unknown2          : 0x%04X\n", ctx->header.unknown2);
  MVM_LOG_I(ctx, "vmgp-summary", "stack_words       : %u (0x%X)\n", ctx->header.stack_words, ctx->header.stack_words);
  MVM_LOG_I(ctx, "vmgp-summary", "code_size         : %u (0x%X)\n", ctx->header.code_size, ctx->header.code_size);
  MVM_LOG_I(ctx, "vmgp-summary", "data_size         : %u (0x%X)\n", ctx->header.data_size, ctx->header.data_size);
  MVM_LOG_I(ctx, "vmgp-summary", "bss_size          : %u (0x%X)\n", ctx->header.bss_size, ctx->header.bss_size);
  MVM_LOG_I(ctx, "vmgp-summary", "res_size          : %u (0x%X)\n", ctx->header.res_size, ctx->header.res_size);
  MVM_LOG_I(ctx, "vmgp-summary", "pool_slots        : %u\n", ctx->header.pool_slots);
  MVM_LOG_I(ctx, "vmgp-summary", "string_table_size : %u (0x%X)\n", ctx->header.string_size, ctx->header.string_size);
  MVM_LOG_I(ctx, "vmgp-summary", "code_offset(vm)   : 0x%X\n", ctx->code_offset);
  MVM_LOG_I(ctx, "vmgp-summary", "code_offset(file) : 0x%X\n", ctx->code_file_offset);
  MVM_LOG_I(ctx, "vmgp-summary", "data_offset       : 0x%X\n", ctx->data_offset);
  MVM_LOG_I(ctx, "vmgp-summary", "bss_offset        : 0x%X\n", ctx->bss_offset);
  MVM_LOG_I(ctx, "vmgp-summary", "res_offset        : 0x%X\n", ctx->res_offset);
  MVM_LOG_I(ctx, "vmgp-summary", "pool_offset(file) : 0x%X\n", ctx->pool_offset);
  MVM_LOG_I(ctx, "vmgp-summary", "strtab_offset     : 0x%X\n", ctx->strtab_offset);
  MVM_LOG_I(ctx, "vmgp-summary", "vm_end            : 0x%X\n", ctx->vm_end);
  MVM_LOG_I(ctx, "vmgp-summary", "heap_base         : 0x%X\n", ctx->heap_base);
  MVM_LOG_I(ctx, "vmgp-summary", "heap_limit        : 0x%X\n", ctx->heap_limit);
  MVM_LOG_I(ctx, "vmgp-summary", "stack_top         : 0x%X\n", ctx->stack_top);
  MVM_LOG_I(ctx, "vmgp-summary", "resource_count    : %u\n", ctx->resource_count);
} /* End of MVM_DumpVmgpSummary */

/**********************************************************************************************************************
 *  Name: MVM_DumpVmgpImports
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
void MVM_DumpVmgpImports(const VMGPContext *ctx, uint32_t max_count)
{
  uint32_t i;
  const VMGPPoolEntry *e = NULL;

  if (!ctx || !ctx->pool)
  {
    return;
  }

  MVM_LOG_I(ctx, "vmgp-imports", "=== imports (leading type=0x02 pool entries) ===\n");

  for (i = 1; i <= ctx->header.pool_slots && i <= max_count; ++i)
  {
    e = MVM_GetVmgpPoolEntry(ctx, i);

    if (!e || e->type != 0x02)
    {
      break;
    }
    MVM_LOG_I(ctx, "vmgp-imports", "[%03u] %s\n", i, MVM_GetVmgpImportName(ctx, i));
  } /* End of loop */
} /* End of MVM_DumpVmgpImports */

/**********************************************************************************************************************
 *  END OF FILE MVM_VmgpDebug.c
 *********************************************************************************************************************/
