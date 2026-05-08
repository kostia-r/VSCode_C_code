#include "VmRunner.h"

#include "MVM_Render.h"
#include "MVM_Trace.h"
#include "MVM_VmgpDebug.h"

#define VM_MAX_STEPS_PER_HOST_FRAME    (10000U)
#define HOST_MIN_LOOP_DELAY_MS         (1U)

static int pump_vm_once(MpnVM_t *vm, uint32_t *step_budget)
{
  MVM_RetCode_t retVal;
  MVM_State_t state;

  state = MVM_GetState(vm);
  if (state != MVM_STATE_READY && state != MVM_STATE_RUNNING)
  {
    return 1;
  }

  if (*step_budget == 0u)
  {
    return 1;
  }

  retVal = MVM_RunStep(vm);
  if (MVM_OK != retVal)
  {
    return 1;
  }

  --(*step_budget);

  return 0;
}

int VmRunner_Run(MpnVM_t *vm, SdlBackend *backend, const VmRunnerOptions *options)
{
  MVM_RenderFrameInfo_t frame_info;
  MVM_RetCode_t retVal;
  uint32_t host_budget;
  uint32_t frame_serial_before;
  uint32_t max_steps;
  uint32_t max_logged_calls;
  uint32_t duration_ms;
  uint32_t start_ms;
  uint32_t step_budget;
  int quit_requested;

  if (!options)
  {
    return 0;
  }

  max_steps = options->max_steps;
  max_logged_calls = options->max_logged_calls;
  duration_ms = options->duration_ms;
  MVM_DumpVmgpSummary(vm);
  MVM_DumpVmgpImports(vm, 64);
  retVal = MVM_SetWdgLimit(vm, 0u);
  if (MVM_OK != retVal)
  {
    return 0;
  }

  SdlBackend_AttachVmTiming(vm, backend);
  SdlBackend_Present(vm, backend);

  start_ms = SdlBackend_GetTicksMs(backend);
  step_budget = max_steps;
  quit_requested = 0;
  while (!quit_requested)
  {
    uint32_t elapsed_ms;

    elapsed_ms = SdlBackend_GetTicksMs(backend) - start_ms;
    if (options->input)
    {
      SdlBackend_SetSyntheticButtons(backend, options->input(options->input_user, elapsed_ms));
    }

    if (SdlBackend_PumpEvents(vm, backend))
    {
      quit_requested = 1;
      break;
    }

    frame_serial_before = 0u;
    if (MVM_RenderGetFrameInfo(vm, &frame_info))
    {
      frame_serial_before = frame_info.frame_serial;
    }

    host_budget = VM_MAX_STEPS_PER_HOST_FRAME;
    while (host_budget != 0u && pump_vm_once(vm, &step_budget) == 0)
    {
      --host_budget;

      if (MVM_RenderGetFrameInfo(vm, &frame_info) && frame_info.frame_serial != frame_serial_before)
      {
        break;
      }
    }

    SdlBackend_Present(vm, backend);

    if (MVM_GetLoggedCalls(vm) >= max_logged_calls)
    {
      break;
    }

    if (MVM_GetState(vm) != MVM_STATE_READY && MVM_GetState(vm) != MVM_STATE_RUNNING)
    {
      break;
    }

    if (step_budget == 0u)
    {
      break;
    }

    if (duration_ms != 0u && elapsed_ms >= duration_ms)
    {
      break;
    }

    SdlBackend_Delay(HOST_MIN_LOOP_DELAY_MS);
  }

  return 1;
}
