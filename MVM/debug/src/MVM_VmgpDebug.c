/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_VmgpDebug.c
 *           Module:  MVM_Debug
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
 *  Name: MVM_vidVmgpDumpSummary
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
void MVM_vidVmgpDumpSummary(const VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  MVM_LvidLogf(ctx, "=== VMGP summary ===\n");
  MVM_LvidLogf(ctx, "magic             : %.4s\n", ctx->header.magic);
  MVM_LvidLogf(ctx, "unknown1          : 0x%04X\n", ctx->header.unknown1);
  MVM_LvidLogf(ctx, "unknown2          : 0x%04X\n", ctx->header.unknown2);
  MVM_LvidLogf(ctx, "stack_words       : %u (0x%X)\n", ctx->header.stack_words, ctx->header.stack_words);
  MVM_LvidLogf(ctx, "code_size         : %u (0x%X)\n", ctx->header.code_size, ctx->header.code_size);
  MVM_LvidLogf(ctx, "data_size         : %u (0x%X)\n", ctx->header.data_size, ctx->header.data_size);
  MVM_LvidLogf(ctx, "bss_size          : %u (0x%X)\n", ctx->header.bss_size, ctx->header.bss_size);
  MVM_LvidLogf(ctx, "res_size          : %u (0x%X)\n", ctx->header.res_size, ctx->header.res_size);
  MVM_LvidLogf(ctx, "pool_slots        : %u\n", ctx->header.pool_slots);
  MVM_LvidLogf(ctx, "string_table_size : %u (0x%X)\n", ctx->header.string_size, ctx->header.string_size);
  MVM_LvidLogf(ctx, "code_offset(vm)   : 0x%X\n", ctx->code_offset);
  MVM_LvidLogf(ctx, "code_offset(file) : 0x%X\n", ctx->code_file_offset);
  MVM_LvidLogf(ctx, "data_offset       : 0x%X\n", ctx->data_offset);
  MVM_LvidLogf(ctx, "bss_offset        : 0x%X\n", ctx->bss_offset);
  MVM_LvidLogf(ctx, "res_offset        : 0x%X\n", ctx->res_offset);
  MVM_LvidLogf(ctx, "pool_offset(file) : 0x%X\n", ctx->pool_offset);
  MVM_LvidLogf(ctx, "strtab_offset     : 0x%X\n", ctx->strtab_offset);
  MVM_LvidLogf(ctx, "vm_end            : 0x%X\n", ctx->vm_end);
  MVM_LvidLogf(ctx, "heap_base         : 0x%X\n", ctx->heap_base);
  MVM_LvidLogf(ctx, "heap_limit        : 0x%X\n", ctx->heap_limit);
  MVM_LvidLogf(ctx, "stack_top         : 0x%X\n", ctx->stack_top);
  MVM_LvidLogf(ctx, "resource_count    : %u\n", ctx->resource_count);
} /* End of MVM_vidVmgpDumpSummary */

/**********************************************************************************************************************
 *  Name: MVM_vidVmgpDumpImports
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
void MVM_vidVmgpDumpImports(const VMGPContext *ctx, uint32_t max_count)
{
  uint32_t i;

  if (!ctx || !ctx->pool)
  {
    return;
  }

  MVM_LvidLogf(ctx, "=== imports (leading type=0x02 pool entries) ===\n");

  for (i = 1; i <= ctx->header.pool_slots && i <= max_count; ++i)
  {
    const VMGPPoolEntry *e = MVM_pudtVmgpGetPoolEntry(ctx, i);

    if (!e || e->type != 0x02)
    {
      break;
    }
    MVM_LvidLogf(ctx, "[%03u] %s\n", i, MVM_pudtVmgpGetImportName(ctx, i));
  } /* End of loop */

} /* End of MVM_vidVmgpDumpImports */

/**********************************************************************************************************************
 *  END OF FILE MVM_VmgpDebug.c
 *********************************************************************************************************************/
