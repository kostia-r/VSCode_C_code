#ifndef MOPHUN_TRACE_H
#define MOPHUN_TRACE_H

#include "mophun_vm.h"

#include <stdbool.h>
#include <stdint.h>

bool mophun_vm_run_trace(MophunVM *vm, uint32_t max_steps, uint32_t max_logged_calls);

#endif
