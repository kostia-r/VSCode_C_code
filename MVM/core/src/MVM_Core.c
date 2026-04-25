/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Core.c
 *           Module:  MVM_Core
 *           Target:  Portable C
 *      Description:  Core VM lifecycle, memory helpers, and platform integration entry points.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include "MVM_BuildCfg.h"
#include "MVM_Cfg.h"
#include "MVM_Syscalls.h"
#include <stdarg.h>
#include <stdio.h>
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
  MophunVM *pudtVm = NULL;

  if (!storage || storage_size < sizeof(MophunVM))
  {
    return NULL;
  }

  if (align != 0u && ((uintptr_t)storage % align) != 0u)
  {
    return NULL;
  }

  pudtVm = (MophunVM *)storage;

  return pudtVm;
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
  return MVM_LbInitRawWithConfig(ctx, data, size, &MVM_kstConfig);
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
  MVM_tstConfig stConfig = MVM_kstConfig;

  if (platform)
  {
    stConfig.platform = *platform;
  }

  return MVM_LbInitRawWithConfig(ctx, data, size, &stConfig);
} /* End of MVM_LbInitRawWithPlatform */

/**********************************************************************************************************************
 *  Name: MVM_LbInitRawWithConfig
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_LbInitRawWithConfig(VMGPContext *ctx,
const uint8_t *data,
size_t size,
const MVM_tstConfig *config)
{
  bool bResult = false;

  if (!ctx || !data || size < sizeof(VMGPHeader))
  {
    return false;
  }

  memset(ctx, 0, sizeof(*ctx));

  if (config)
  {
    ctx->platform = config->platform;
    ctx->device_profile = config->device_profile;
    ctx->syscalls = config->syscalls;
    ctx->syscall_count = config->syscall_count;
    ctx->runtime_pool = (uint8_t *)config->runtime_pool;
    ctx->runtime_pool_size = config->runtime_pool_size;
    ctx->watchdog_limit = config->watchdog_limit;
  }
  ctx->data = data;
  ctx->size = size;
  ctx->next_stream_handle = 0x30u;
  ctx->random_state = 1u;
  ctx->last_pc = UINT32_MAX;
  if (!config)
  {
    ctx->watchdog_limit = MVM_U32_DEFAULT_WATCHDOG_LIMIT;
  }
  ctx->state = MVM_TENU_STATE_READY;
  ctx->last_error = MVM_TENU_ERROR_NONE;
  bResult = true;

  return bResult;
} /* End of MVM_LbInitRawWithConfig */

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
  return MVM_bInitWithConfig(vm, image, image_size, &MVM_kstConfig);
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
  MVM_tstConfig stConfig = MVM_kstConfig;

  if (platform)
  {
    stConfig.platform = *platform;
  }

  return MVM_bInitWithConfig(vm, image, image_size, &stConfig);
} /* End of MVM_bInitWithPlatform */

/**********************************************************************************************************************
 *  Name: MVM_bInitWithConfig
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_bInitWithConfig(MophunVM *vm,
const uint8_t *image,
size_t image_size,
const MVM_tstConfig *config)
{
  bool bResult = false;
  const MVM_tstConfig *pstConfig = NULL;

  pstConfig = config ? config : &MVM_kstConfig;

  if (!MVM_LbInitRawWithConfig(vm, image, image_size, pstConfig))
  {
    MVM_LvidSetError(vm, MVM_TENU_ERROR_INIT_FAILED);

    return false;
  }

  if (!MVM_bVmgpParseHeader(vm) || !MVM_bVmgpLoadPool(vm))
  {
    if (vm->last_error == MVM_TENU_ERROR_NONE)
    {
      MVM_LvidSetError(vm, MVM_TENU_ERROR_INIT_FAILED);
    }
    MVM_LvidFreeRaw(vm);

    return false;
  }

  bResult = true;

  return bResult;
} /* End of MVM_bInitWithConfig */

/**********************************************************************************************************************
 *  Name: MVM_LpudtAcquireInitBuffer
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Acquires one initialization buffer from static config.
 *********************************************************************************************************************/
void *MVM_LpudtAcquireInitBuffer(VMGPContext *ctx, size_t required_size)
{
  size_t udtAlignedOffset = 0;
  size_t udtEndOffset = 0;
  void *pudtMem = NULL;

  if (!ctx || required_size == 0u)
  {
    return NULL;
  }

  if (!ctx->runtime_pool || ctx->runtime_pool_size == 0u)
  {
    return NULL;
  }

  udtAlignedOffset = (ctx->runtime_pool_used + 3u) & ~(size_t)3u;
  udtEndOffset = udtAlignedOffset + required_size;

  if (udtEndOffset < udtAlignedOffset || udtEndOffset > ctx->runtime_pool_size)
  {
    return NULL;
  }

  pudtMem = ctx->runtime_pool + udtAlignedOffset;
  memset(pudtMem, 0, required_size);
  ctx->runtime_pool_used = udtEndOffset;

  return pudtMem;
} /* End of MVM_LpudtAcquireInitBuffer */

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
 *  Name: MVM_LvidSetState
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Updates the current VM execution state.
 *********************************************************************************************************************/
void MVM_LvidSetState(VMGPContext *ctx, MVM_tenuState state)
{
  if (!ctx)
  {
    return;
  }

  ctx->state = state;
} /* End of MVM_LvidSetState */

/**********************************************************************************************************************
 *  Name: MVM_LvidSetError
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Records the last fatal execution error and stops execution.
 *********************************************************************************************************************/
void MVM_LvidSetError(VMGPContext *ctx, MVM_tenuError error)
{
  if (!ctx)
  {
    return;
  }

  ctx->last_error = error;
  ctx->halted = true;
  ctx->state = MVM_TENU_STATE_ERROR;
} /* End of MVM_LvidSetError */

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
 *  Name: MVM_bStep
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes at most one VM instruction.
 *********************************************************************************************************************/
bool MVM_bStep(MophunVM *vm)
{
  uint32_t u32Executed = 0;
  bool bResult = false;

  u32Executed = MVM_u32RunSteps(vm, 1u);
  bResult = (vm != NULL) && (u32Executed == 1u);

  return bResult;
} /* End of MVM_bStep */

/**********************************************************************************************************************
 *  Name: MVM_u32RunSteps
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes up to the requested VM instruction budget.
 *********************************************************************************************************************/
uint32_t MVM_u32RunSteps(MophunVM *vm, uint32_t max_steps)
{
  uint32_t u32Executed = 0;
  uint32_t u32PcBefore = 0;

  if (!vm)
  {
    return 0;
  }

  if (max_steps == 0u)
  {
    return 0;
  }

  if (vm->state == MVM_TENU_STATE_ERROR ||
      vm->state == MVM_TENU_STATE_EXITED ||
      vm->state == MVM_TENU_STATE_PAUSED ||
      vm->state == MVM_TENU_STATE_WAITING)
  {
    return 0;
  }

  vm->state = MVM_TENU_STATE_RUNNING;

  while (u32Executed < max_steps)
  {
    if (vm->halted)
    {
      if (vm->state != MVM_TENU_STATE_ERROR)
      {
        vm->state = MVM_TENU_STATE_EXITED;
      }
      break;
    }

    u32PcBefore = vm->pc;

    if (!MVM_LbPipStep(vm))
    {
      if (vm->last_error == MVM_TENU_ERROR_NONE)
      {
        MVM_LvidSetError(vm, MVM_TENU_ERROR_EXECUTION);
      }
      break;
    }

    ++u32Executed;

    if (vm->pc == u32PcBefore)
    {
      ++vm->no_progress_steps;
    }
    else
    {
      vm->no_progress_steps = 0u;
    }

    vm->last_pc = vm->pc;

    if (vm->watchdog_limit != 0u && vm->no_progress_steps >= vm->watchdog_limit)
    {
      MVM_LvidSetError(vm, MVM_TENU_ERROR_WATCHDOG);
      break;
    }
  } /* End of loop */

  if (vm->state == MVM_TENU_STATE_RUNNING)
  {
    vm->state = vm->halted ? MVM_TENU_STATE_EXITED : MVM_TENU_STATE_READY;
  }

  return u32Executed;
} /* End of MVM_u32RunSteps */

/**********************************************************************************************************************
 *  Name: MVM_u32RunForTime
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes VM instructions for up to the requested host time budget.
 *********************************************************************************************************************/
uint32_t MVM_u32RunForTime(MophunVM *vm, uint32_t budget_ms)
{
  uint32_t u32Executed = 0;
  uint32_t u32Start = 0;
  uint32_t u32Now = 0;

  if (!vm || budget_ms == 0u)
  {
    return 0;
  }

  if (!vm->platform.get_ticks_ms)
  {
    u32Executed = MVM_u32RunSteps(vm, budget_ms);

    return u32Executed;
  }

  u32Start = vm->platform.get_ticks_ms(vm->platform.user);
  u32Now = u32Start;

  while ((u32Now - u32Start) < budget_ms)
  {
    if (MVM_u32RunSteps(vm, 1u) == 0u)
    {
      break;
    }

    ++u32Executed;

    if (vm->state != MVM_TENU_STATE_READY && vm->state != MVM_TENU_STATE_RUNNING)
    {
      break;
    }

    u32Now = vm->platform.get_ticks_ms(vm->platform.user);
  } /* End of loop */

  return u32Executed;
} /* End of MVM_u32RunForTime */

/**********************************************************************************************************************
 *  Name: MVM_vidPause
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Pauses VM execution.
 *********************************************************************************************************************/
void MVM_vidPause(MophunVM *vm)
{
  if (!vm)
  {
    return;
  }

  if (vm->state == MVM_TENU_STATE_READY || vm->state == MVM_TENU_STATE_RUNNING)
  {
    vm->state = MVM_TENU_STATE_PAUSED;
  }
} /* End of MVM_vidPause */

/**********************************************************************************************************************
 *  Name: MVM_vidWait
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Moves VM execution into a host-wait state.
 *********************************************************************************************************************/
void MVM_vidWait(MophunVM *vm)
{
  if (!vm)
  {
    return;
  }

  if (vm->state == MVM_TENU_STATE_READY || vm->state == MVM_TENU_STATE_RUNNING)
  {
    vm->state = MVM_TENU_STATE_WAITING;
  }
} /* End of MVM_vidWait */

/**********************************************************************************************************************
 *  Name: MVM_vidResume
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Resumes VM execution.
 *********************************************************************************************************************/
void MVM_vidResume(MophunVM *vm)
{
  if (!vm)
  {
    return;
  }

  if (vm->state == MVM_TENU_STATE_PAUSED || vm->state == MVM_TENU_STATE_WAITING)
  {
    vm->state = MVM_TENU_STATE_READY;
  }
} /* End of MVM_vidResume */

/**********************************************************************************************************************
 *  Name: MVM_vidRequestExit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Requests immediate VM termination.
 *********************************************************************************************************************/
void MVM_vidRequestExit(MophunVM *vm)
{
  if (!vm)
  {
    return;
  }

  vm->halted = true;
  vm->state = MVM_TENU_STATE_EXITED;
} /* End of MVM_vidRequestExit */

/**********************************************************************************************************************
 *  Name: MVM_tenuGetState
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the current VM execution state.
 *********************************************************************************************************************/
MVM_tenuState MVM_tenuGetState(const MophunVM *vm)
{
  MVM_tenuState enuState = MVM_TENU_STATE_ERROR;

  enuState = vm ? vm->state : MVM_TENU_STATE_ERROR;

  return enuState;
} /* End of MVM_tenuGetState */

/**********************************************************************************************************************
 *  Name: MVM_tenuGetLastError
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the last fatal VM execution error.
 *********************************************************************************************************************/
MVM_tenuError MVM_tenuGetLastError(const MophunVM *vm)
{
  MVM_tenuError enuError = MVM_TENU_ERROR_INVALID_ARG;

  enuError = vm ? vm->last_error : MVM_TENU_ERROR_INVALID_ARG;

  return enuError;
} /* End of MVM_tenuGetLastError */

/**********************************************************************************************************************
 *  Name: MVM_vidSetWatchdogLimit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Configures the no-progress soft watchdog limit.
 *********************************************************************************************************************/
void MVM_vidSetWatchdogLimit(MophunVM *vm, uint32_t no_progress_steps)
{
  if (!vm)
  {
    return;
  }

  vm->watchdog_limit = no_progress_steps;
  vm->no_progress_steps = 0u;
  vm->last_pc = UINT32_MAX;
} /* End of MVM_vidSetWatchdogLimit */

/**********************************************************************************************************************
 *  Name: MVM_u32GetWatchdogLimit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the configured no-progress soft watchdog limit.
 *********************************************************************************************************************/
uint32_t MVM_u32GetWatchdogLimit(const MophunVM *vm)
{
  uint32_t u32Limit = 0;

  u32Limit = vm ? vm->watchdog_limit : 0u;

  return u32Limit;
} /* End of MVM_u32GetWatchdogLimit */

/**********************************************************************************************************************
 *  Name: MVM_u32GetExecutedSteps
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the total number of executed VM instructions.
 *********************************************************************************************************************/
uint32_t MVM_u32GetExecutedSteps(const MophunVM *vm)
{
  uint32_t u32Steps = 0;

  u32Steps = vm ? vm->steps : 0u;

  return u32Steps;
} /* End of MVM_u32GetExecutedSteps */

/**********************************************************************************************************************
 *  Name: MVM_u32GetProgramCounter
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the current VM program counter.
 *********************************************************************************************************************/
uint32_t MVM_u32GetProgramCounter(const MophunVM *vm)
{
  uint32_t u32Pc = 0;

  u32Pc = vm ? vm->pc : 0u;

  return u32Pc;
} /* End of MVM_u32GetProgramCounter */

/**********************************************************************************************************************
 *  Name: MVM_u32GetLoggedCalls
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the number of trace calls logged so far.
 *********************************************************************************************************************/
uint32_t MVM_u32GetLoggedCalls(const MophunVM *vm)
{
  uint32_t u32LoggedCalls = 0;

  u32LoggedCalls = vm ? vm->logged_calls : 0u;

  return u32LoggedCalls;
} /* End of MVM_u32GetLoggedCalls */

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
