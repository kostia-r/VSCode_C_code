#ifndef VM_RUNNER_H
#define VM_RUNNER_H

#include "MVM.h"
#include "SdlBackend.h"
#include <stdint.h>

int VmRunner_Run(MpnVM_t *vm, SdlBackend *backend, uint32_t max_steps, uint32_t max_logged_calls);

#endif
