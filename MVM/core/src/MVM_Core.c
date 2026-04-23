/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Core.c
 *           Module:  MVM_Core
 *           Target:  Portable C
 *      Description:  Mophun VM component source.
 *            Notes:  Structured according to project styling guidelines.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include "MVM_Config.h"
#include "MVM_Syscalls.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_udtGetStorageSize
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
size_t MVM_udtGetStorageSize(void)
{
  return sizeof(MophunVM);
} /* End of MVM_udtGetStorageSize */

/**********************************************************************************************************************
 *  Name: MVM_udtGetStorageAlign
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
size_t MVM_udtGetStorageAlign(void)
{
  typedef struct MophunVMAlignProbe
  {
    char c;
    MophunVM vm;
  } MophunVMAlignProbe;

  return offsetof(MophunVMAlignProbe, vm);
} /* End of MVM_udtGetStorageAlign */

/**********************************************************************************************************************
 *  Name: MVM_pudtGetVmFromStorage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
MophunVM *MVM_pudtGetVmFromStorage(void *storage, size_t storage_size)
{
  size_t align = MVM_udtGetStorageAlign();

  if (!storage || storage_size < sizeof(MophunVM))
  {
    return NULL;
  }

  if (align != 0u && ((uintptr_t)storage % align) != 0u)
  {
    return NULL;
  }

  return (MophunVM *)storage;
} /* End of MVM_pudtGetVmFromStorage */

/**********************************************************************************************************************
 *  Name: MVM_LbInitRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_LbInitRaw(VMGPContext *ctx, const uint8_t *data, size_t size)
{
  return MVM_LbInitRawWithPlatform(ctx, data, size, NULL);
} /* End of MVM_LbInitRaw */

/**********************************************************************************************************************
 *  Name: MVM_LbInitRawWithPlatform
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_LbInitRawWithPlatform(VMGPContext *ctx, const uint8_t *data, size_t size, const MophunPlatform *platform)
{
  if (!ctx || !data || size < sizeof(VMGPHeader))
  {
    return false;
  }
  memset(ctx, 0, sizeof(*ctx));

  if (platform)
  {
    ctx->platform = *platform;
  }
  ctx->data = data;
  ctx->size = size;
  ctx->next_stream_handle = 0x30u;
  ctx->random_state = 1u;
  return true;
} /* End of MVM_LbInitRawWithPlatform */

/**********************************************************************************************************************
 *  Name: MVM_bInit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_bInit(MophunVM *vm, const uint8_t *image, size_t image_size)
{
  return MVM_bInitWithPlatform(vm, image, image_size, NULL);
} /* End of MVM_bInit */

/**********************************************************************************************************************
 *  Name: MVM_bInitWithPlatform
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_bInitWithPlatform(MophunVM *vm, const uint8_t *image, size_t image_size, const MophunPlatform *platform)
{
  if (!MVM_LbInitRawWithPlatform(vm, image, image_size, platform))
  {
    return false;
  }

  if (!MVM_bVmgpParseHeader(vm) || !MVM_bVmgpLoadPool(vm))
  {
    MVM_LvidFreeRaw(vm);
    return false;
  }

  return true;
} /* End of MVM_bInitWithPlatform */

/**********************************************************************************************************************
 *  Name: MVM_LpudtCalloc
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides local VM helper logic.
 *********************************************************************************************************************/
void *MVM_LpudtCalloc(VMGPContext *ctx, size_t count, size_t size)
{
  if (ctx && ctx->platform.calloc)
  {
    return ctx->platform.calloc(ctx->platform.user, count, size);
  }

#if MVM_ENABLE_DEFAULT_ALLOCATOR
  return calloc(count, size);
#else
  (void)ctx;
  (void)count;
  (void)size;
  return NULL;
#endif
} /* End of MVM_LpudtCalloc */

/**********************************************************************************************************************
 *  Name: MVM_LvidFreeMem
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Releases VM resources.
 *********************************************************************************************************************/
void MVM_LvidFreeMem(VMGPContext *ctx, void *ptr)
{
  if (!ptr)
  {
    return;
  }

  if (ctx && ctx->platform.free)
  {
    ctx->platform.free(ctx->platform.user, ptr);
    return;
  }
#if MVM_ENABLE_DEFAULT_ALLOCATOR
  free(ptr);
#else
  (void)ctx;
#endif
} /* End of MVM_LvidFreeMem */

/**********************************************************************************************************************
 *  Name: MVM_LvidLogf
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Emits diagnostic trace output.
 *********************************************************************************************************************/
void MVM_LvidLogf(const VMGPContext *ctx, const char *fmt, ...)
{
  char buffer[MVM_LOG_BUFFER_SIZE];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);

  if (ctx && ctx->platform.log)
  {
    ctx->platform.log(ctx->platform.user, buffer);
    return;
  }

#if MVM_ENABLE_DEFAULT_LOGGER
  fputs(buffer, stdout);
#else
  (void)ctx;
#endif
} /* End of MVM_LvidLogf */

/**********************************************************************************************************************
 *  Name: MVM_LvidFreeRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Releases VM resources.
 *********************************************************************************************************************/
void MVM_LvidFreeRaw(VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  MVM_LvidFreeMem(ctx, ctx->pool);
  MVM_LvidFreeMem(ctx, ctx->resources);
  MVM_LvidFreeMem(ctx, ctx->mem);
  memset(ctx, 0, sizeof(*ctx));
} /* End of MVM_LvidFreeRaw */

/**********************************************************************************************************************
 *  Name: MVM_vidFree
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Releases VM resources.
 *********************************************************************************************************************/
void MVM_vidFree(MophunVM *vm)
{
  MVM_LvidFreeRaw(vm);
} /* End of MVM_vidFree */

/**********************************************************************************************************************
 *  Name: MVM_vidSetSyscalls
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
void MVM_vidSetSyscalls(MophunVM *vm, const MophunSyscall *syscalls, uint32_t count)
{
  if (!vm)
  {
    return;
  }

  vm->syscalls = syscalls;
  vm->syscall_count = count;
} /* End of MVM_vidSetSyscalls */

/**********************************************************************************************************************
 *  END OF FILE MVM_Core.c
 *********************************************************************************************************************/
