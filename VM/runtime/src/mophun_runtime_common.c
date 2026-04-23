#include "mophun_vm_internal.h"

bool mophun_runtime_mem_range_ok(const VMGPContext *ctx, uint32_t addr, uint32_t size)
{
  return ctx && addr <= ctx->mem_size && size <= ctx->mem_size - addr;
}

uint32_t mophun_runtime_strlen(const uint8_t *s, size_t max_len)
{
  uint32_t n = 0;
  while (n < max_len && s[n] != 0)
  {
    ++n;
  }
  return n;
}
