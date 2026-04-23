#ifndef MOPHUN_VMGP_DEBUG_H
#define MOPHUN_VMGP_DEBUG_H

#include "mophun_vm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VMGP_MAGIC "VMGP"
#define VMGP_POOL_SLOT_SIZE 8u

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

bool vmgp_parse_header(MophunVM *vm);
bool vmgp_load_pool(MophunVM *vm);

const VMGPPoolEntry *vmgp_get_pool_entry(const MophunVM *vm, uint32_t pool_index_1based);
const char *vmgp_pool_type_name(uint8_t type);
const char *vmgp_get_import_name(const MophunVM *vm, uint32_t pool_index_1based);
size_t vmgp_pool_size_bytes(const VMGPHeader *header);

void vmgp_dump_summary(const MophunVM *vm);
void vmgp_dump_imports(const MophunVM *vm, uint32_t max_count);

#endif
