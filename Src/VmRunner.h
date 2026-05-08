#ifndef VM_RUNNER_H
#define VM_RUNNER_H

#include "MVM.h"
#include "SdlBackend.h"
#include <stdint.h>

typedef uint32_t (*VmRunnerInputFn)(void *user, uint32_t elapsed_ms);

typedef struct VmRunnerOptions
{
  uint32_t max_steps;
  uint32_t max_logged_calls;
  uint32_t duration_ms;
  VmRunnerInputFn input;
  void *input_user;
} VmRunnerOptions;

int VmRunner_Run(MpnVM_t *vm, SdlBackend *backend, const VmRunnerOptions *options);

#endif
