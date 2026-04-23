#ifndef MOPHUN_VM_INTERNAL_H
#define MOPHUN_VM_INTERNAL_H

#include "mophun_vm.h"
#include "mophun_syscalls.h"
#include "mophun_vmgp_debug.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VMGP_MAX_REGS 32
#define VMGP_MAX_STREAMS 16
#define VM_STACK_EXTRA (64u * 1024u)
#define VM_HEAP_EXTRA (128u * 1024u)

/* PIP2 register ABI */
enum
{
  VM_REG_ZERO = 0,
  VM_REG_SP = 1,
  VM_REG_RA = 2,
  VM_REG_FP = 3,
  VM_REG_P0 = 12,
  VM_REG_P1 = 13,
  VM_REG_P2 = 14,
  VM_REG_P3 = 15,
  VM_REG_R0 = 30,
  VM_REG_R1 = 31,
};

typedef struct VMGPStream
{
  bool used;
  uint32_t handle;
  uint32_t base;
  uint32_t size;
  uint32_t pos;
  uint32_t resource_id;
} VMGPStream;

struct MophunVM
{
  MophunPlatform platform;

  const uint8_t *data;
  size_t size;

  VMGPHeader header;
  bool header_valid;

  uint32_t code_offset;
  uint32_t code_file_offset;
  uint32_t data_offset;
  uint32_t data_file_offset;
  uint32_t bss_offset;
  uint32_t res_offset;
  uint32_t res_file_offset;
  uint32_t pool_offset;
  uint32_t strtab_offset;
  uint32_t vm_end;

  VMGPPoolEntry *pool;
  VMGPResource *resources;
  uint32_t resource_count;

  uint8_t *mem;
  size_t mem_size;
  uint32_t heap_base;
  uint32_t heap_cur;
  uint32_t heap_limit;
  uint32_t stack_top;

  VMGPStream streams[VMGP_MAX_STREAMS];
  uint32_t next_stream_handle;

  uint32_t regs[VMGP_MAX_REGS];
  uint32_t pc;
  uint32_t steps;
  uint32_t logged_calls;
  uint32_t tick_count;
  uint32_t random_state;
  bool halted;

  const MophunSyscall *syscalls;
  uint32_t syscall_count;
};

typedef MophunVM VMGPContext;

static inline uint16_t vm_read_u16_le(const uint8_t *p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t vm_read_u32_le(const uint8_t *p)
{
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static inline void vm_write_u32_le(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline void vm_write_u16_le(uint8_t *p, uint16_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline uint32_t vm_reg_index(uint8_t encoded)
{
  return (uint32_t)(encoded >> 2);
}

static inline uint32_t vm_imm24_u(uint32_t v)
{
  return v & 0x00FFFFFFu;
}

static inline int32_t vm_sext24(uint32_t v)
{
  v &= 0x00FFFFFFu;
  if (v & 0x00800000u)
  {
    v |= 0xFF000000u;
  }
  return (int32_t)v;
}

static inline int32_t vm_reg_s32(uint32_t v)
{
  return (int32_t)v;
}

static inline uint32_t vm_align4(uint32_t v)
{
  return (v + 3u) & ~3u;
}

uint32_t vmgp_resolve_pool_value(const VMGPContext *ctx, const VMGPPoolEntry *entry);
const VMGPResource *vmgp_get_resource(const VMGPContext *ctx, uint32_t resource_id);
void mophun_vm_memory_write_watch(const VMGPContext *ctx, uint32_t addr, uint32_t size, const char *tag);
bool vmgp_handle_import_call(VMGPContext *ctx, uint32_t pool_index);
bool vmgp_init(VMGPContext *ctx, const uint8_t *data, size_t size);
bool vmgp_init_with_platform(VMGPContext *ctx,
                             const uint8_t *data,
                             size_t size,
                             const MophunPlatform *platform);
void vmgp_free(VMGPContext *ctx);
bool vmgp_run_trace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls);
bool vmgp_step(VMGPContext *ctx);

void *mophun_vm_calloc(VMGPContext *ctx, size_t count, size_t size);
void mophun_vm_free_mem(VMGPContext *ctx, void *ptr);
void mophun_vm_logf(const VMGPContext *ctx, const char *fmt, ...);

bool mophun_runtime_mem_range_ok(const VMGPContext *ctx, uint32_t addr, uint32_t size);
uint32_t mophun_runtime_strlen(const uint8_t *s, size_t max_len);
bool mophun_runtime_handle_stream(VMGPContext *ctx, const char *name);
bool mophun_runtime_handle_caps(VMGPContext *ctx, const char *name);
bool mophun_runtime_handle_decompress(VMGPContext *ctx, const char *name);
bool mophun_runtime_handle_heap(VMGPContext *ctx, const char *name);
bool mophun_runtime_handle_time_random(VMGPContext *ctx, const char *name);
bool mophun_runtime_handle_strings(VMGPContext *ctx, const char *name);
bool mophun_runtime_handle_misc(VMGPContext *ctx, const char *name);

#endif
