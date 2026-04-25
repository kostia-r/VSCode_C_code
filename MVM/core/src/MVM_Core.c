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
#include <string.h>

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Maps one internal fatal VM error to one public API return code.
 */
static MVM_RetCode_t MVM_lMapFatalError(MVM_Err_t error, MVM_RetCode_t fallback);

/**
 * @brief Finds one configured device profile by name.
 */
static const MpnDevProfile_t *MVM_lFindDevProfileByName(const char *profile_name);

/**
 * @brief Initializes VM state using one internal integration config object.
 */
static MVM_RetCode_t MVM_lInitWithConfig(MpnVM_t *vm,
                                         const MpnImageSource_t *image,
                                         const MVM_Config_t *config);

/**
 * @brief Reads one byte range from a memory-backed VM image.
 */
static int MVM_lReadMemoryImage(void *user, size_t offset, void *dst, size_t size);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_GetStorageSize
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
size_t MVM_GetStorageSize(void)
{
  return sizeof(MpnVM_t);
} /* End of MVM_GetStorageSize */

/**********************************************************************************************************************
 *  Name: MVM_GetStorageAlign
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
size_t MVM_GetStorageAlign(void)
{
  typedef struct MophunVMAlignProbe
  {
    char c;
    MpnVM_t vm;
  } MophunVMAlignProbe;

  return offsetof(MophunVMAlignProbe, vm);
} /* End of MVM_GetStorageAlign */

/**********************************************************************************************************************
 *  Name: MVM_GetVmFromStorage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
MpnVM_t *MVM_GetVmFromStorage(void *storage, size_t storage_size)
{
  size_t align = MVM_GetStorageAlign();
  MpnVM_t *vm = NULL;

  if (!storage || storage_size < sizeof(MpnVM_t))
  {
    return NULL;
  }

  if (align != 0u && ((uintptr_t)storage % align) != 0u)
  {
    return NULL;
  }

  vm = (MpnVM_t *)storage;

  return vm;
} /* End of MVM_GetVmFromStorage */

/**********************************************************************************************************************
 *  Name: MVM_InitRawWithConfig
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_InitRawWithConfig(VMGPContext *ctx, const MpnImageSource_t *image, const MVM_Config_t *config)
{
  bool bResult = false;

  if (!ctx || !image || image->image_size < sizeof(VMGPHeader))
  {
    return false;
  }

  if (config && !config->image_read)
  {
    return false;
  }

  memset(ctx, 0, sizeof(*ctx));

  if (config)
  {
    ctx->platform = config->platform;
    ctx->image_read = config->image_read;
    ctx->image_map = config->image_map;
    ctx->image_unmap = config->image_unmap;
    ctx->device_profile = config->device_profile;
    ctx->syscalls = config->syscalls;
    ctx->syscall_count = config->syscall_count;
    ctx->runtime_pool = (uint8_t *)config->runtime_pool;
    ctx->runtime_pool_size = config->runtime_pool_size;
    ctx->watchdog_limit = config->watchdog_limit;
  }
  else
  {
    ctx->image_read = NULL;
    ctx->image_map = NULL;
    ctx->image_unmap = NULL;
  }
  ctx->image = *image;
  ctx->size = image->image_size;
  ctx->next_stream_handle = 0x30u;
  ctx->random_state = 1u;
  ctx->last_pc = UINT32_MAX;
  if (!config)
  {
    ctx->watchdog_limit = MVM_U32_DEFAULT_WATCHDOG_LIMIT;
  }
  ctx->state = MVM_STATE_READY;
  ctx->last_error = MVM_E_NONE;
  bResult = true;

  return bResult;
} /* End of MVM_InitRawWithConfig */

/**********************************************************************************************************************
 *  Name: MVM_ReadImageRange
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Reads one byte range from the active image source through the configured backend.
 *********************************************************************************************************************/
bool MVM_ReadImageRange(const VMGPContext *ctx, size_t offset, void *dst, size_t size)
{
  if (!ctx || !ctx->image_read || !dst)
  {
    return false;
  }

  if (size == 0u)
  {
    return true;
  }

  if (offset > ctx->size || size > (ctx->size - offset))
  {
    return false;
  }

  return ctx->image_read(ctx->image.user, offset, dst, size) == 0;
} /* End of MVM_ReadImageRange */

/**********************************************************************************************************************
 *  Name: MVM_InitFromSource
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state through one image source.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_InitFromSource(MpnVM_t *vm, const MpnImageSource_t *image, const char *profile_name)
{
  MVM_Config_t config = MVM_Config;
  const MpnDevProfile_t *profile = NULL;

  if (profile_name)
  {
    profile = MVM_lFindDevProfileByName(profile_name);
    if (!profile)
    {
      return MVM_NOT_FOUND;
    }

    config.device_profile = profile;
  }

  return MVM_lInitWithConfig(vm, image, &config);
} /* End of MVM_InitFromSource */

/**********************************************************************************************************************
 *  Name: MVM_Init
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state with the built-in integration config.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_Init(MpnVM_t *vm,
                       const uint8_t *image,
                       size_t image_size,
                       const char *profile_name)
{
  MpnImageSource_t source;
  MVM_Config_t config = MVM_Config;
  const MpnDevProfile_t *profile = NULL;

  if (!image || image_size < sizeof(VMGPHeader))
  {
    return MVM_INVALID_ARG;
  }

  if (profile_name)
  {
    profile = MVM_lFindDevProfileByName(profile_name);

    if (!profile)
    {
      return MVM_NOT_FOUND;
    }

    config.device_profile = profile;
  }

  config.image_read = MVM_lReadMemoryImage;
  config.image_map = NULL;
  config.image_unmap = NULL;

  memset(&source, 0, sizeof(source));
  source.user = (void *)image;
  source.image_size = image_size;

  return MVM_lInitWithConfig(vm, &source, &config);
} /* End of MVM_Init */

/**********************************************************************************************************************
 *  Name: MVM_GetDevProfileCount
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the number of built-in device profiles.
 *********************************************************************************************************************/
uint32_t MVM_GetDevProfileCount(void)
{
  return MVM_Config.device_profile_count;
} /* End of MVM_GetDevProfileCount */

/**********************************************************************************************************************
 *  Name: MVM_GetDevProfile
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns one built-in device profile by index.
 *********************************************************************************************************************/
const MpnDevProfile_t *MVM_GetDevProfile(uint32_t profile_index)
{
  if (profile_index >= MVM_Config.device_profile_count)
  {
    return NULL;
  }

  return &MVM_Config.device_profiles[profile_index];
} /* End of MVM_GetDevProfile */

/**********************************************************************************************************************
 *  Name: MVM_FindDevProfileByName
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Finds one built-in device profile by name.
 *********************************************************************************************************************/
const MpnDevProfile_t *MVM_FindDevProfileByName(const char *profile_name)
{
  return MVM_lFindDevProfileByName(profile_name);
} /* End of MVM_FindDevProfileByName */

/**********************************************************************************************************************
 *  Name: MVM_AcquireInitBuffer
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Acquires one initialization buffer from static config.
 *********************************************************************************************************************/
void *MVM_AcquireInitBuffer(VMGPContext *ctx, size_t required_size)
{
  size_t alignedOffset = 0;
  size_t endOffset = 0;
  void *mem = NULL;

  if (!ctx || required_size == 0u)
  {
    return NULL;
  }

  if (!ctx->runtime_pool || ctx->runtime_pool_size == 0u)
  {
    return NULL;
  }

  alignedOffset = (ctx->runtime_pool_used + 3u) & ~(size_t)3u;
  endOffset = alignedOffset + required_size;

  if (endOffset < alignedOffset || endOffset > ctx->runtime_pool_size)
  {
    return NULL;
  }

  mem = ctx->runtime_pool + alignedOffset;
  memset(mem, 0, required_size);
  ctx->runtime_pool_used = endOffset;

  return mem;
} /* End of MVM_AcquireInitBuffer */

/**********************************************************************************************************************
 *  Name: MVM_EmitEvent
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Emits one structured VM event through the platform hook.
 *********************************************************************************************************************/
void MVM_EmitEvent(const VMGPContext *ctx, MVM_Event_t event, uint32_t arg0, uint32_t arg1)
{
  if (!ctx)
  {
    return;
  }

#if (MVM_MAX_LOG_LEVEL >= 3U)
  MVM_LOG_EVT(ctx, event, arg0, arg1);
#endif

  if (ctx->platform.event)
  {
    ctx->platform.event(ctx->platform.user, event, arg0, arg1);
  }
} /* End of MVM_EmitEvent */

/**********************************************************************************************************************
 *  Name: MVM_SetStateRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Updates the current VM execution state.
 *********************************************************************************************************************/
void MVM_SetStateRaw(VMGPContext *ctx, MVM_State_t state)
{
  if (!ctx)
  {
    return;
  }

  ctx->state = state;
} /* End of MVM_SetStateRaw */

/**********************************************************************************************************************
 *  Name: MVM_SetErrorRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Records the last fatal execution error and stops execution.
 *********************************************************************************************************************/
void MVM_SetErrorRaw(VMGPContext *ctx, MVM_Err_t error)
{
  if (!ctx)
  {
    return;
  }

  ctx->last_error = error;
  ctx->halted = true;
  ctx->state = MVM_STATE_ERROR;
  MVM_EmitEvent(ctx, MVM_EVENT_VM_ERROR, (uint32_t)error, ctx->pc);
} /* End of MVM_SetErrorRaw */

/**********************************************************************************************************************
 *  Name: MVM_FreeRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Releases VM resources.
 *********************************************************************************************************************/
void MVM_FreeRaw(VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  memset(ctx, 0, sizeof(*ctx));
} /* End of MVM_FreeRaw */

/**********************************************************************************************************************
 *  Name: MVM_Free
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Releases VM resources.
 *********************************************************************************************************************/
void MVM_Free(MpnVM_t *vm)
{
  MVM_FreeRaw(vm);
} /* End of MVM_Free */

/**********************************************************************************************************************
 *  Name: MVM_RunStepsRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes up to the requested VM instruction budget through the internal path.
 *********************************************************************************************************************/
uint32_t MVM_RunStepsRaw(VMGPContext *vm, uint32_t max_steps)
{
  uint32_t executed = 0;
  uint32_t pcBefore = 0;

  if (!vm)
  {
    return 0;
  }

  if (max_steps == 0u)
  {
    return 0;
  }

  if (vm->state == MVM_STATE_ERROR ||
      vm->state == MVM_STATE_EXITED ||
      vm->state == MVM_STATE_PAUSED ||
      vm->state == MVM_STATE_WAITING)
  {
    return 0;
  }

  vm->state = MVM_STATE_RUNNING;

  while (executed < max_steps)
  {
    if (vm->halted)
    {
      if (vm->state != MVM_STATE_ERROR)
      {
        vm->state = MVM_STATE_EXITED;
      }
      break;
    }

    pcBefore = vm->pc;

    if (!MVM_PipStep(vm))
    {
      if (vm->last_error == MVM_E_NONE)
      {
        MVM_SetErrorRaw(vm, MVM_E_EXECUTION);
      }
      break;
    }

    ++executed;

    if (vm->pc == pcBefore)
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
      MVM_SetErrorRaw(vm, MVM_E_WDG);
      break;
    }
  } /* End of loop */

  if (vm->state == MVM_STATE_RUNNING)
  {
    vm->state = vm->halted ? MVM_STATE_EXITED : MVM_STATE_READY;
  }

  return executed;
} /* End of MVM_RunStepsRaw */

/**********************************************************************************************************************
 *  Name: MVM_RunStep
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes at most one VM instruction.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_RunStep(MpnVM_t *vm)
{
  uint32_t executed = 0;

  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  executed = MVM_RunStepsRaw(vm, 1u);
  if (executed == 1u)
  {
    return MVM_OK;
  }

  if (vm->state == MVM_STATE_PAUSED ||
      vm->state == MVM_STATE_WAITING ||
      vm->state == MVM_STATE_EXITED)
  {
    return MVM_BAD_STATE;
  }

  return MVM_lMapFatalError(vm->last_error, MVM_EXECUTION_ERROR);
} /* End of MVM_RunStep */

/**********************************************************************************************************************
 *  Name: MVM_RunSteps
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes up to the requested VM instruction budget.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_RunSteps(MpnVM_t *vm, uint32_t max_steps, uint32_t *executed_steps)
{
  uint32_t executed = 0;

  if (!vm || !executed_steps || max_steps == 0u)
  {
    return MVM_INVALID_ARG;
  }

  executed = MVM_RunStepsRaw(vm, max_steps);
  *executed_steps = executed;

  if (executed > 0u)
  {
    return MVM_OK;
  }

  if (vm->state == MVM_STATE_PAUSED ||
      vm->state == MVM_STATE_WAITING ||
      vm->state == MVM_STATE_EXITED)
  {
    return MVM_BAD_STATE;
  }

  return MVM_lMapFatalError(vm->last_error, MVM_EXECUTION_ERROR);
} /* End of MVM_RunSteps */

/**********************************************************************************************************************
 *  Name: MVM_RunForTime
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes VM instructions for up to the requested host time budget.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_RunForTime(MpnVM_t *vm, uint32_t budget_ms, uint32_t *executed_steps)
{
  uint32_t executed = 0;
  uint32_t start = 0;
  uint32_t now = 0;

  if (!vm || !executed_steps || budget_ms == 0u)
  {
    return MVM_INVALID_ARG;
  }

  *executed_steps = 0u;

  if (!vm->platform.get_ticks_ms)
  {
    *executed_steps = MVM_RunStepsRaw(vm, budget_ms);

    if (*executed_steps > 0u)
    {
      return MVM_OK;
    }

    if (vm->state == MVM_STATE_PAUSED ||
        vm->state == MVM_STATE_WAITING ||
        vm->state == MVM_STATE_EXITED)
    {
      return MVM_BAD_STATE;
    }

    return MVM_lMapFatalError(vm->last_error, MVM_EXECUTION_ERROR);
  }

  start = vm->platform.get_ticks_ms(vm->platform.user);
  now = start;

  while ((now - start) < budget_ms)
  {
    if (MVM_RunStepsRaw(vm, 1u) == 0u)
    {
      break;
    }

    ++executed;

    if (vm->state != MVM_STATE_READY && vm->state != MVM_STATE_RUNNING)
    {
      break;
    }

    now = vm->platform.get_ticks_ms(vm->platform.user);
  } /* End of loop */

  *executed_steps = executed;

  if (executed > 0u)
  {
    return MVM_OK;
  }

  if (vm->state == MVM_STATE_PAUSED ||
      vm->state == MVM_STATE_WAITING ||
      vm->state == MVM_STATE_EXITED)
  {
    return MVM_BAD_STATE;
  }

  return MVM_lMapFatalError(vm->last_error, MVM_EXECUTION_ERROR);
} /* End of MVM_RunForTime */

/**********************************************************************************************************************
 *  Name: MVM_Pause
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Pauses VM execution.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_Pause(MpnVM_t *vm)
{
  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  if (vm->state == MVM_STATE_READY || vm->state == MVM_STATE_RUNNING)
  {
    vm->state = MVM_STATE_PAUSED;
    MVM_EmitEvent(vm, MVM_EVENT_VM_PAUSED, vm->pc, 0u);

    return MVM_OK;
  }

  return MVM_BAD_STATE;
} /* End of MVM_Pause */

/**********************************************************************************************************************
 *  Name: MVM_Wait
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Moves VM execution into a host-wait state.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_Wait(MpnVM_t *vm)
{
  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  if (vm->state == MVM_STATE_READY || vm->state == MVM_STATE_RUNNING)
  {
    vm->state = MVM_STATE_WAITING;
    MVM_EmitEvent(vm, MVM_EVENT_VM_WAITING, vm->pc, 0u);

    return MVM_OK;
  }

  return MVM_BAD_STATE;
} /* End of MVM_Wait */

/**********************************************************************************************************************
 *  Name: MVM_Resume
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Resumes VM execution.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_Resume(MpnVM_t *vm)
{
  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  if (vm->state == MVM_STATE_PAUSED || vm->state == MVM_STATE_WAITING)
  {
    vm->state = MVM_STATE_READY;
    MVM_EmitEvent(vm, MVM_EVENT_VM_RESUMED, vm->pc, 0u);

    return MVM_OK;
  }

  return MVM_BAD_STATE;
} /* End of MVM_Resume */

/**********************************************************************************************************************
 *  Name: MVM_RequestExitRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Requests immediate VM termination through the internal path.
 *********************************************************************************************************************/
void MVM_RequestExitRaw(VMGPContext *vm)
{
  if (!vm)
  {
    return;
  }

  vm->halted = true;
  vm->state = MVM_STATE_EXITED;
  MVM_EmitEvent(vm, MVM_EVENT_VM_EXITED, vm->pc, 0u);
} /* End of MVM_RequestExitRaw */

/**********************************************************************************************************************
 *  Name: MVM_RequestExit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Requests immediate VM termination.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_RequestExit(MpnVM_t *vm)
{
  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  MVM_RequestExitRaw(vm);

  return MVM_OK;
} /* End of MVM_RequestExit */

/**********************************************************************************************************************
 *  Name: MVM_GetState
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the current VM execution state.
 *********************************************************************************************************************/
MVM_State_t MVM_GetState(const MpnVM_t *vm)
{
  MVM_State_t state = MVM_STATE_ERROR;

  state = vm ? vm->state : MVM_STATE_ERROR;

  return state;
} /* End of MVM_GetState */

/**********************************************************************************************************************
 *  Name: MVM_GetLastError
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the last fatal VM execution error.
 *********************************************************************************************************************/
MVM_Err_t MVM_GetLastError(const MpnVM_t *vm)
{
  MVM_Err_t error = MVM_E_INVALID_ARG;

  error = vm ? vm->last_error : MVM_E_INVALID_ARG;

  return error;
} /* End of MVM_GetLastError */

/**********************************************************************************************************************
 *  Name: MVM_SetWdgLimit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Configures the no-progress soft watchdog limit.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_SetWdgLimit(MpnVM_t *vm, uint32_t no_progress_steps)
{
  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  vm->watchdog_limit = no_progress_steps;
  vm->no_progress_steps = 0u;
  vm->last_pc = UINT32_MAX;

  return MVM_OK;
} /* End of MVM_SetWdgLimit */

/**********************************************************************************************************************
 *  Name: MVM_GetWdgLimit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the configured no-progress soft watchdog limit.
 *********************************************************************************************************************/
uint32_t MVM_GetWdgLimit(const MpnVM_t *vm)
{
  uint32_t limit = 0;

  limit = vm ? vm->watchdog_limit : 0u;

  return limit;
} /* End of MVM_GetWdgLimit */

/**********************************************************************************************************************
 *  Name: MVM_GetExecutedSteps
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the total number of executed VM instructions.
 *********************************************************************************************************************/
uint32_t MVM_GetExecutedSteps(const MpnVM_t *vm)
{
  uint32_t steps = 0;

  steps = vm ? vm->steps : 0u;

  return steps;
} /* End of MVM_GetExecutedSteps */

/**********************************************************************************************************************
 *  Name: MVM_GetProgramCounter
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the current VM program counter.
 *********************************************************************************************************************/
uint32_t MVM_GetProgramCounter(const MpnVM_t *vm)
{
  uint32_t pc = 0;

  pc = vm ? vm->pc : 0u;

  return pc;
} /* End of MVM_GetProgramCounter */

/**********************************************************************************************************************
 *  Name: MVM_GetLoggedCalls
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns the number of trace calls logged so far.
 *********************************************************************************************************************/
uint32_t MVM_GetLoggedCalls(const MpnVM_t *vm)
{
  uint32_t loggedCalls = 0;

  loggedCalls = vm ? vm->logged_calls : 0u;

  return loggedCalls;
} /* End of MVM_GetLoggedCalls */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_lMapFatalError
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Maps one internal fatal VM error to one public API return code.
 *********************************************************************************************************************/
static MVM_RetCode_t MVM_lMapFatalError(MVM_Err_t error, MVM_RetCode_t fallback)
{
  MVM_RetCode_t retCode = fallback;

  switch (error)
  {
    case MVM_E_NONE:
    {
      retCode = fallback;
      break;
    }

    case MVM_E_INVALID_ARG:
    {
      retCode = MVM_INVALID_ARG;
      break;
    }

    case MVM_E_INIT_FAILED:
    {
      retCode = MVM_INIT_FAILED;
      break;
    }

    case MVM_E_MEMORY:
    {
      retCode = MVM_MEMORY_ERROR;
      break;
    }

    case MVM_E_EXECUTION:
    {
      retCode = MVM_EXECUTION_ERROR;
      break;
    }

    case MVM_E_WDG:
    {
      retCode = MVM_WATCHDOG_ERROR;
      break;
    }

    default:
    {
      break;
    }
  }

  return retCode;
} /* End of MVM_lMapFatalError */

/**********************************************************************************************************************
 *  Name: MVM_lFindDevProfileByName
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Finds one configured device profile by name.
 *********************************************************************************************************************/
static const MpnDevProfile_t *MVM_lFindDevProfileByName(const char *profile_name)
{
  return MVM_Cfg_lFindDevProfileByName(&MVM_Config, profile_name);
} /* End of MVM_lFindDevProfileByName */

/**********************************************************************************************************************
 *  Name: MVM_lInitWithConfig
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state using one internal integration config object.
 *********************************************************************************************************************/
static MVM_RetCode_t MVM_lInitWithConfig(MpnVM_t *vm,
                                         const MpnImageSource_t *image,
                                         const MVM_Config_t *config)
{
  MVM_RetCode_t retCode = MVM_OK;
  const MVM_Config_t *cfg = config;

  if (!vm || !image || image->image_size < sizeof(VMGPHeader) || !cfg || !cfg->image_read)
  {
    return MVM_INVALID_ARG;
  }

  if (!MVM_InitRawWithConfig(vm, image, cfg))
  {
    MVM_SetErrorRaw(vm, MVM_E_INIT_FAILED);

    return MVM_INIT_FAILED;
  }

  if (!MVM_ParseVmgpHeaderRaw(vm) || !MVM_LoadVmgpPoolRaw(vm))
  {
    if (vm->last_error == MVM_E_NONE)
    {
      MVM_SetErrorRaw(vm, MVM_E_INIT_FAILED);
    }

    retCode = MVM_lMapFatalError(vm->last_error, MVM_INIT_FAILED);

    return retCode;
  }

  return retCode;
} /* End of MVM_lInitWithConfig */

/**********************************************************************************************************************
 *  Name: MVM_lReadMemoryImage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Reads one byte range from a memory-backed VM image.
 *********************************************************************************************************************/
static int MVM_lReadMemoryImage(void *user, size_t offset, void *dst, size_t size)
{
  const uint8_t *image = (const uint8_t *)user;

  if (!image || !dst)
  {
    return -1;
  }

  memcpy(dst, image + offset, size);

  return 0;
} /* End of MVM_lReadMemoryImage */

/**********************************************************************************************************************
 *  END OF FILE MVM_Core.c
 *********************************************************************************************************************/
