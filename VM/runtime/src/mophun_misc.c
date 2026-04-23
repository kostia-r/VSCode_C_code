#include "mophun_vm_internal.h"

#include <string.h>

bool mophun_runtime_handle_misc(VMGPContext *ctx, const char *name)
{
  if (strcmp(name, "vTerminateVMGP") == 0)
  {
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  if (strcmp(name, "DbgPrintf") == 0 || strcmp(name, "vPrint") == 0)
  {
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  return false;
}
