/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  OpenMophun
 *             File:  MVM_Core.c
 *           Module:  MVM_Core
 *           Target:  Portable C
 *      Description:  Core VM lifecycle, memory helpers, and platform integration entry points.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include "MVM_BuildConfig.h"
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
static MVM_RetCode_t MVM_lInitInstance(MpnVM_t *vm, const MVM_InitConfig_t *config);

/**
 * @brief Allocates the optional VM-owned driver framebuffer from the runtime pool.
 */
static bool MVM_lInitDriverFramebuffer(MpnVM_t *vm);

/**
 * @brief Initializes the SDK default screen palette from RGB332 indices.
 */
static void MVM_lInitDefaultPalette(VMGPContext *ctx);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_GetInstanceStorageSize
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
size_t MVM_GetInstanceStorageSize(void)
{
  return sizeof(MpnVM_t);
} /* End of MVM_GetInstanceStorageSize */

/**********************************************************************************************************************
 *  Name: MVM_GetInstanceStorageAlign
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
size_t MVM_GetInstanceStorageAlign(void)
{
  typedef struct MVM_AlignProbe
  {
    char c;
    MpnVM_t vm;
  } MVM_AlignProbe;

  return offsetof(MVM_AlignProbe, vm);
} /* End of MVM_GetInstanceStorageAlign */

bool MVM_ValidateConfig(const MVM_InitConfig_t *config, bool require_runtime_pool)
{
  if (!config || !config->image_path || config->image_path[0] == '\0' ||
      !config->file_api || !config->file_api->open ||
      !config->file_api->read || !config->file_api->close ||
      config->profile.screen_width == 0U ||
      config->profile.screen_height == 0U ||
      config->log_level > MVM_LOG_LEVEL_TRACE)
  {
    return false;
  }

  if (config->file_api->resize && !config->file_api->write)
  {
    return false;
  }

  if (require_runtime_pool && (!config->runtime_pool || config->runtime_pool_size == 0U))
  {
    return false;
  }

  return true;
} /* End of MVM_ValidateConfig */

/**********************************************************************************************************************
 *  Name: MVM_lGetInstanceFromStorage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static MpnVM_t *MVM_lGetInstanceFromStorage(void *storage, size_t storage_size)
{
  size_t align = MVM_GetInstanceStorageAlign();
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
} /* End of MVM_lGetInstanceFromStorage */

/**********************************************************************************************************************
 *  Name: MVM_InitRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state.
 *********************************************************************************************************************/
bool MVM_InitRaw(VMGPContext *ctx, const MVM_InitConfig_t *config)
{
  bool bResult = false;

  if (!ctx || !MVM_ValidateConfig(config, true))
  {
    return false;
  }

  memset(ctx, 0, sizeof(*ctx));

  ctx->drivers = config->services;
  ctx->log_level = config->log_level;
  if (ctx->drivers.get_ticks_ms)
  {
    ctx->log_start_ms = ctx->drivers.get_ticks_ms(ctx->drivers.context);
  }
  ctx->image_path = config->image_path;
  ctx->file_api = *config->file_api;
  ctx->device_profile_storage = config->profile;
  ctx->device_profile = &ctx->device_profile_storage;
  ctx->runtime_pool = (uint8_t *)config->runtime_pool;
  ctx->runtime_pool_size = config->runtime_pool_size;
  ctx->watchdog_limit = config->watchdog_limit;
  ctx->image.handle = MVM_FILE_INVALID_HANDLE;
  if (ctx->file_api.open(ctx->file_api.context,
                         ctx->image_path,
                         NULL,
                         MVM_FILE_OPEN_READ | (ctx->file_api.write ? MVM_FILE_OPEN_WRITE : 0U),
                         &ctx->image.handle,
                         &ctx->image.image_size) != 0 ||
      ctx->image.handle == MVM_FILE_INVALID_HANDLE ||
      ctx->image.image_size < VMGP_HEADER_SIZE)
  {
    return false;
  }
  ctx->size = ctx->image.image_size;
  ctx->next_stream_handle = 0x30u;
  ctx->random_state = 1u;
  ctx->last_pc = UINT32_MAX;
  MVM_lInitDefaultPalette(ctx);
  ctx->state = MVM_STATE_READY;
  ctx->last_error = MVM_E_NONE;
  bResult = true;

  return bResult;
} /* End of MVM_InitRaw */

static void MVM_lInitDefaultPalette(VMGPContext *ctx)
{
  uint32_t index;

  if (!ctx)
  {
    return;
  }

  for (index = 0u; index < 256u; ++index)
  {
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    red = (((index >> 5u) & 0x07u) * 31u) / 7u;
    green = (((index >> 2u) & 0x07u) * 31u) / 7u;
    blue = ((index & 0x03u) * 31u) / 3u;
    ctx->palette_entries[index] = (red << 10u) | (green << 5u) | blue;
  }
} /* End of MVM_lInitDefaultPalette */

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
  size_t readSize;

  if (!ctx || !ctx->file_api.read || !dst)
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

  readSize = 0U;
  return ctx->file_api.read(ctx->file_api.context, ctx->image.handle, offset, dst, size, &readSize) == 0 &&
         readSize == size;
} /* End of MVM_ReadImageRange */

/**********************************************************************************************************************
 *  Name: MVM_WriteImageRange
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Writes one byte range to the active image source through the configured backend.
 *********************************************************************************************************************/
bool MVM_WriteImageRange(const VMGPContext *ctx, size_t offset, const void *src, size_t size)
{
  size_t writtenSize;

  if (!ctx || !ctx->file_api.write || !src)
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

  writtenSize = 0U;
  return ctx->file_api.write(ctx->file_api.context, ctx->image.handle, offset, src, size, &writtenSize) == 0 &&
         writtenSize == size;
} /* End of MVM_WriteImageRange */

/**********************************************************************************************************************
 *  Name: MVM_Init
 *  Description: Initializes one isolated VM instance in parent-owned storage.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_Init(void *instance_storage,
                       size_t instance_storage_size,
                       const MVM_InitConfig_t *config,
                       MVM_Instance_t **out_instance)
{
  MpnVM_t *vm;
  MVM_RetCode_t result;

  if (out_instance)
  {
    *out_instance = NULL;
  }

  if (!out_instance || !MVM_ValidateConfig(config, true))
  {
    return MVM_INVALID_ARG;
  }

  vm = MVM_lGetInstanceFromStorage(instance_storage, instance_storage_size);
  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  result = MVM_lInitInstance(vm, config);
  if (result != MVM_OK)
  {
    MVM_FreeRaw(vm);
    return result;
  }

  *out_instance = (MVM_Instance_t *)vm;
  return MVM_OK;
} /* End of MVM_Init */

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

  MVM_LOG_EVT(ctx, event, arg0, arg1);

  if (ctx->drivers.event)
  {
    ctx->drivers.event(ctx->drivers.context, event, arg0, arg1);
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
  uint32_t index;

  if (!ctx)
  {
    return;
  }

  (void)MVM_FlushPersistentData(ctx);
  if (ctx->file_api.close)
  {
    for (index = 0U; index < VMGP_MAX_STREAMS; ++index)
    {
      if (ctx->streams[index].used && ctx->streams[index].file_handle != MVM_FILE_INVALID_HANDLE)
      {
        (void)ctx->file_api.close(ctx->file_api.context, ctx->streams[index].file_handle);
      }
    }
    if (ctx->image.handle != MVM_FILE_INVALID_HANDLE)
    {
      (void)ctx->file_api.close(ctx->file_api.context, ctx->image.handle);
    }
  }
  memset(ctx, 0, sizeof(*ctx));
} /* End of MVM_FreeRaw */

/**********************************************************************************************************************
 *  Name: MVM_Deinit
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Releases VM resources.
 *********************************************************************************************************************/
void MVM_Deinit(MpnVM_t *vm)
{
  MVM_FreeRaw(vm);
} /* End of MVM_Deinit */

MVM_RetCode_t MVM_GetHeapStats(const MpnVM_t *vm, MVM_HeapStats_t *stats)
{
  if (!vm || !stats)
  {
    return MVM_INVALID_ARG;
  }

  stats->capacity_bytes = vm->heap_limit - vm->heap_base;
  stats->soft_limit_bytes = vm->heap_soft_limit - vm->heap_base;
  stats->high_water_bytes = vm->heap_high_water_bytes;
  stats->allocation_requests = vm->heap_allocation_requests;
  stats->free_requests = vm->heap_free_requests;
  stats->allocation_failures = vm->heap_allocation_failures;
  stats->invalid_free_requests = vm->heap_invalid_free_requests;
  stats->double_free_requests = vm->heap_double_free_requests;
  stats->live_bytes = vm->heap_live_bytes;
  stats->peak_live_bytes = vm->heap_peak_live_bytes;
  stats->quarantine_bytes = vm->heap_quarantine_bytes;
  stats->tracker_overflows = vm->heap_tracker_overflows;
  stats->reuse_count = vm->heap_reuse_count;
  stats->soft_limit_fallbacks = vm->heap_soft_limit_fallbacks;

  return MVM_OK;
} /* End of MVM_GetHeapStats */

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
 *  Name: MVM_RunFrame
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes until one frame is ready or the instruction budget is exhausted.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_RunFrame(MpnVM_t *vm, uint32_t max_steps, uint32_t *executed_steps)
{
  uint32_t executed;
  uint32_t frame_serial;

  if (!vm || !executed_steps || max_steps == 0U)
  {
    return MVM_INVALID_ARG;
  }

  executed = 0U;
  frame_serial = vm->frame_serial;

  /* Execute until the guest presents one frame or consumes the safety budget. */
  while (executed < max_steps && vm->frame_serial == frame_serial)
  {
    if (MVM_RunStepsRaw(vm, 1U) == 0U)
    {
      break;
    }

    ++executed;
  }

  *executed_steps = executed;

  if (executed > 0U)
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
} /* End of MVM_RunFrame */

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

  if (!vm->drivers.get_ticks_ms)
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

  start = vm->drivers.get_ticks_ms(vm->drivers.context);
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

    now = vm->drivers.get_ticks_ms(vm->drivers.context);
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
 *  Name: MVM_SetFixedDateTime
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Configures a deterministic date/time returned by runtime date imports.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_SetFixedDateTime(MpnVM_t *vm,
                                   uint16_t year,
                                   uint8_t month,
                                   uint8_t day,
                                   uint8_t hour,
                                   uint8_t minute,
                                   uint8_t second)
{
  if (!vm)
  {
    return MVM_INVALID_ARG;
  }

  if (year == 0u)
  {
    vm->fixed_time_year = 0u;
    vm->fixed_time_month = 0u;
    vm->fixed_time_day = 0u;
    vm->fixed_time_hour = 0u;
    vm->fixed_time_minute = 0u;
    vm->fixed_time_second = 0u;

    return MVM_OK;
  }

  if (year < 1970u || month < 1u || month > 12u || day < 1u || day > 31u ||
      hour > 23u || minute > 59u || second > 59u)
  {
    return MVM_INVALID_ARG;
  }

  vm->fixed_time_year = year;
  vm->fixed_time_month = month;
  vm->fixed_time_day = day;
  vm->fixed_time_hour = hour;
  vm->fixed_time_minute = minute;
  vm->fixed_time_second = second;

  return MVM_OK;
} /* End of MVM_SetFixedDateTime */

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
 *  Name: MVM_lInitInstance
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Initializes VM state using one internal integration config object.
 *********************************************************************************************************************/
static MVM_RetCode_t MVM_lInitInstance(MpnVM_t *vm, const MVM_InitConfig_t *config)
{
  MVM_RetCode_t retCode = MVM_OK;
  size_t allocatorMetadataBytes;

  if (!vm || !MVM_ValidateConfig(config, true))
  {
    return MVM_INVALID_ARG;
  }

  if (!MVM_InitRaw(vm, config))
  {
    MVM_SetErrorRaw(vm, MVM_E_INIT_FAILED);

    return MVM_INIT_FAILED;
  }

  allocatorMetadataBytes = (size_t)VMGP_MAX_HEAP_TRACKED_ALLOCATIONS * sizeof(MVM_HeapAllocation_t);
  vm->heap_allocations = (MVM_HeapAllocation_t *)MVM_AcquireInitBuffer(vm, allocatorMetadataBytes);
  if (!vm->heap_allocations)
  {
    MVM_SetErrorRaw(vm, MVM_E_MEMORY);
    return MVM_MEMORY_ERROR;
  }
  memset(vm->heap_allocations, 0, allocatorMetadataBytes);

  if (!MVM_ParseVmgpHeaderRaw(vm) || !MVM_LoadVmgpPoolRaw(vm))
  {
    if (vm->last_error == MVM_E_NONE)
    {
      MVM_SetErrorRaw(vm, MVM_E_INIT_FAILED);
    }

    retCode = MVM_lMapFatalError(vm->last_error, MVM_INIT_FAILED);

    return retCode;
  }

  if (!MVM_lInitDriverFramebuffer(vm))
  {
    MVM_SetErrorRaw(vm, MVM_E_MEMORY);

    return MVM_MEMORY_ERROR;
  }

  return retCode;
} /* End of MVM_lInitInstance */

/**********************************************************************************************************************
 *  Name: MVM_lInitDriverFramebuffer
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Allocates the optional RGB565 framebuffer used by the hardware-oriented display driver.
 *********************************************************************************************************************/
static bool MVM_lInitDriverFramebuffer(MpnVM_t *vm)
{
  size_t pixelCount = 0U;
  size_t framebufferBytes = 0U;

  if (!vm || !vm->drivers.display_flush)
  {
    return true;
  }

  if (!vm->device_profile || vm->device_profile->screen_width == 0U || vm->device_profile->screen_height == 0U)
  {
    return false;
  }

  pixelCount = (size_t)vm->device_profile->screen_width * (size_t)vm->device_profile->screen_height;
  if (pixelCount > (SIZE_MAX / sizeof(uint16_t)))
  {
    return false;
  }

  framebufferBytes = pixelCount * sizeof(uint16_t);
  vm->driver_framebuffer = (uint16_t *)MVM_AcquireInitBuffer(vm, framebufferBytes);
  if (!vm->driver_framebuffer)
  {
    return false;
  }

  vm->driver_framebuffer_width = vm->device_profile->screen_width;
  vm->driver_framebuffer_height = vm->device_profile->screen_height;
  vm->driver_framebuffer_clear_serial = UINT32_MAX;

  return true;
} /* End of MVM_lInitDriverFramebuffer */

/**********************************************************************************************************************
 *  END OF FILE MVM_Core.c
 *********************************************************************************************************************/
