#ifndef VMGP_PARSER_H
#define VMGP_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VMGP_MAGIC "VMGP"
#define VMGP_POOL_SLOT_SIZE 8u
#define VMGP_MAX_REGS 32
#define VMGP_MAX_STREAMS 16

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

typedef struct VMGPHeader
{
  char magic[4];
  uint16_t unknown1;
  uint16_t unknown2;
  uint16_t stack_words;
  uint8_t unknown3;
  uint8_t unknown4;
  uint32_t code_size;
  uint32_t data_size;
  uint32_t bss_size;
  uint32_t res_size;
  uint32_t unknown5;
  uint32_t pool_slots;
  uint32_t string_size;
} VMGPHeader;

typedef struct VMGPPoolEntry
{
  uint8_t type;
  uint32_t aux24;
  uint32_t value;
} VMGPPoolEntry;

typedef struct VMGPResource
{
  uint32_t id;
  uint32_t offset;
  uint32_t size;
} VMGPResource;

typedef struct VMGPStream
{
  bool used;
  uint32_t handle;
  uint32_t base;
  uint32_t size;
  uint32_t pos;
  uint32_t resource_id;
} VMGPStream;

typedef struct VMGPContext
{
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
  bool halted;
} VMGPContext;

bool vmgp_init(VMGPContext *ctx, const uint8_t *data, size_t size);
void vmgp_free(VMGPContext *ctx);

bool vmgp_parse_header(VMGPContext *ctx);
bool vmgp_load_pool(VMGPContext *ctx);

const VMGPPoolEntry *vmgp_get_pool_entry(const VMGPContext *ctx, uint32_t pool_index_1based);
const char *vmgp_pool_type_name(uint8_t type);
const char *vmgp_get_import_name(const VMGPContext *ctx, uint32_t pool_index_1based);
size_t vmgp_pool_size_bytes(const VMGPHeader *header);

void vmgp_dump_summary(const VMGPContext *ctx);
void vmgp_dump_imports(const VMGPContext *ctx, uint32_t max_count);

bool vmgp_run_trace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls);

#endif
