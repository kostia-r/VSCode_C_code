#ifndef VMGP_PARSER_H
#define VMGP_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VMGP_HEADER_SIZE_GUESS 0x100u
#define VMGP_MAGIC "VMGP"
#define VMGP_POOL_SLOT_SIZE 8u

typedef struct VMGPHeader
{
  char magic[4];        /* "VMGP" */
  uint32_t field_04;    /* unknown: flags/version/file id? */
  uint32_t header_size; /* observed: 0x100 */

  uint32_t code_size; /* bytes */
  uint32_t data_size; /* initialized data bytes */
  uint32_t bss_size;  /* zero-initialized data bytes */
  uint32_t res_size;  /* resource section bytes */

  uint32_t reserved0; /* often 0 */

  uint32_t pool_slots;  /* pool units, 1 slot = 8 bytes */
  uint32_t string_size; /* bytes */
} VMGPHeader;

typedef struct SectionTagRecord
{
  uint8_t prefix;     /* observed 0x00 */
  char tag[3];        /* "GFX", "JMA", "PRG", ... */
  uint16_t type_code; /* e.g. 0x0401 for GFX */
  uint16_t reserved;
} SectionTagRecord;

typedef struct VMGPImport
{
  uint8_t kind;       /* usually 0x02 */
  uint8_t name_off_0; /* 24-bit LE offset into import string table */
  uint8_t name_off_1;
  uint8_t name_off_2;
  uint32_t resolved; /* runtime-patched slot/address, 0 in file */
} VMGPImport;

typedef struct VMGPContext
{
  const uint8_t *data;
  size_t size;

  VMGPHeader header;
  bool header_valid;

  size_t code_offset;
  size_t code_size;
  size_t code_end_offset;

  size_t section_tags_offset;
  size_t section_tag_count;

  size_t imports_offset;
  size_t import_count;

  size_t import_strtab_offset;
  size_t import_strtab_size;
} VMGPContext;

bool vmgp_init(VMGPContext *ctx, const uint8_t *data, size_t size);
bool vmgp_parse_header(VMGPContext *ctx);
bool vmgp_find_section_tags(VMGPContext *ctx);
bool vmgp_find_imports_and_strtab(VMGPContext *ctx);

const SectionTagRecord *vmgp_get_section_tag(const VMGPContext *ctx, size_t index);
const VMGPImport *vmgp_get_import(const VMGPContext *ctx, size_t index);

uint32_t vmgp_import_name_offset(const VMGPImport *imp);
const char *vmgp_get_import_name(const VMGPContext *ctx, size_t import_index);
size_t vmgp_pool_size_bytes(const VMGPHeader *header);

void vmgp_dump_summary(const VMGPContext *ctx);

#endif