#include "vmgp_parser.h"

#include <stdio.h>
#include <string.h>

static uint32_t read_u32_le(const uint8_t *p)
{
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static bool is_printable_ascii(char c)
{
  return (c >= 0x20 && c <= 0x7e);
}

static bool match_section_tag_record(const uint8_t *p)
{
  /* Observed format:
     [00]['G']['F']['X'][01][04][00][00] */
  if (p[0] != 0x00)
  {
    return false;
  }

  if (!is_printable_ascii((char)p[1]) ||
      !is_printable_ascii((char)p[2]) ||
      !is_printable_ascii((char)p[3]))
  {
    return false;
  }

  /* In the observed sample the low byte of type_code is 0x01. */
  if (p[4] != 0x01)
  {
    return false;
  }

  return true;
}

static bool is_known_tag3(const char tag[3])
{
  return (memcmp(tag, "GFX", 3) == 0) ||
         (memcmp(tag, "JMA", 3) == 0) ||
         (memcmp(tag, "PRG", 3) == 0) ||
         (memcmp(tag, "SCA", 3) == 0) ||
         (memcmp(tag, "RY ", 3) == 0) ||
         (memcmp(tag, "---", 3) == 0);
}

static bool looks_like_import_name(const char *s, size_t max_len)
{
  size_t len = 0;

  if (max_len == 0 || s[0] == '\0')
  {
    return false;
  }

  /* Most observed names start with 'v'. The trailer may start with '~'. */
  if (s[0] != 'v' && s[0] != '~')
  {
    return false;
  }

  while (len < max_len && s[len] != '\0')
  {
    if (!is_printable_ascii(s[len]))
    {
      return false;
    }
    ++len;
  }

  return (len > 1 && len < max_len);
}

bool vmgp_init(VMGPContext *ctx, const uint8_t *data, size_t size)
{
  if (!ctx || !data || size < VMGP_HEADER_SIZE_GUESS)
  {
    return false;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->data = data;
  ctx->size = size;
  return true;
}

bool vmgp_parse_header(VMGPContext *ctx)
{
  if (!ctx || !ctx->data || ctx->size < 0x28)
  {
    return false;
  }

  memcpy(ctx->header.magic, ctx->data + 0x00, 4);
  ctx->header.field_04 = read_u32_le(ctx->data + 0x04);
  ctx->header.header_size = read_u32_le(ctx->data + 0x08);
  ctx->header.code_size = read_u32_le(ctx->data + 0x0C);
  ctx->header.data_size = read_u32_le(ctx->data + 0x10);
  ctx->header.bss_size = read_u32_le(ctx->data + 0x14);
  ctx->header.res_size = read_u32_le(ctx->data + 0x18);
  ctx->header.reserved0 = read_u32_le(ctx->data + 0x1C);
  ctx->header.pool_slots = read_u32_le(ctx->data + 0x20);
  ctx->header.string_size = read_u32_le(ctx->data + 0x24);

  ctx->header_valid = (memcmp(ctx->header.magic, VMGP_MAGIC, 4) == 0);
  if (!ctx->header_valid)
  {
    return false;
  }

  ctx->code_offset = (size_t)ctx->header.header_size;
  ctx->code_size = (size_t)ctx->header.code_size;
  ctx->code_end_offset = ctx->code_offset + ctx->code_size;

  if (ctx->code_offset > ctx->size || ctx->code_end_offset > ctx->size)
  {
    return false;
  }

  return true;
}

bool vmgp_find_section_tags(VMGPContext *ctx)
{
  if (!ctx || !ctx->data)
  {
    return false;
  }

  for (size_t off = 0; off + 8 * 4 < ctx->size; ++off)
  {
    const uint8_t *p = ctx->data + off;
    if (!match_section_tag_record(p))
    {
      continue;
    }

    {
      char tag0[3] = {(char)p[1], (char)p[2], (char)p[3]};
      if (!is_known_tag3(tag0))
      {
        continue;
      }
    }

    {
      size_t count = 0;
      size_t cur = off;

      while (cur + 8 <= ctx->size)
      {
        const uint8_t *r = ctx->data + cur;
        char tag[3];

        if (!match_section_tag_record(r))
        {
          break;
        }

        tag[0] = (char)r[1];
        tag[1] = (char)r[2];
        tag[2] = (char)r[3];

        if (!is_known_tag3(tag))
        {
          break;
        }

        ++count;
        cur += 8;

        if (count >= 6)
        {
          break;
        }
      }

      if (count >= 4)
      {
        ctx->section_tags_offset = off;
        ctx->section_tag_count = count;
        return true;
      }
    }
  }

  return false;
}

bool vmgp_find_imports_and_strtab(VMGPContext *ctx)
{
  const char needle[] = "vClearScreen";

  if (!ctx || !ctx->data)
  {
    return false;
  }

  {
    size_t str_off = 0;
    bool found = false;

    for (size_t i = 0; i + sizeof(needle) < ctx->size; ++i)
    {
      if (memcmp(ctx->data + i, needle, sizeof(needle) - 1) == 0)
      {
        str_off = i;
        found = true;
        break;
      }
    }

    if (!found || str_off == 0)
    {
      return false;
    }

    /* In the observed sample import 0 points at offset 0x000001. */
    ctx->import_strtab_offset = str_off - 1;
    ctx->import_strtab_size = ctx->size - ctx->import_strtab_offset;

    {
      size_t best_off = 0;
      size_t best_count = 0;

      for (size_t start = str_off; start >= 8; --start)
      {
        size_t cur = start;
        size_t count = 0;

        while (cur >= 8)
        {
          const uint8_t *p;
          VMGPImport imp;
          uint32_t noff;
          const char *name;
          size_t remain;

          cur -= 8;
          p = ctx->data + cur;

          imp.kind = p[0];
          imp.name_off_0 = p[1];
          imp.name_off_1 = p[2];
          imp.name_off_2 = p[3];
          imp.resolved = read_u32_le(p + 4);

          if (imp.kind != 0x02)
          {
            break;
          }

          noff = vmgp_import_name_offset(&imp);
          if (ctx->import_strtab_offset + noff >= ctx->size)
          {
            break;
          }

          name = (const char *)(ctx->data + ctx->import_strtab_offset + noff);
          remain = ctx->size - (ctx->import_strtab_offset + noff);

          if (!looks_like_import_name(name, remain))
          {
            break;
          }

          ++count;
        }

        if (count > best_count)
        {
          best_count = count;
          best_off = start - best_count * 8;
        }

        if (start == 8)
        {
          break;
        }
      }

      if (best_count == 0)
      {
        return false;
      }

      ctx->imports_offset = best_off;
      ctx->import_count = best_count;
      return true;
    }
  }
}

const SectionTagRecord *vmgp_get_section_tag(const VMGPContext *ctx, size_t index)
{
  if (!ctx || ctx->section_tag_count == 0 || index >= ctx->section_tag_count)
  {
    return NULL;
  }

  return (const SectionTagRecord *)(ctx->data + ctx->section_tags_offset + index * 8);
}

const VMGPImport *vmgp_get_import(const VMGPContext *ctx, size_t index)
{
  if (!ctx || ctx->import_count == 0 || index >= ctx->import_count)
  {
    return NULL;
  }

  return (const VMGPImport *)(ctx->data + ctx->imports_offset + index * 8);
}

uint32_t vmgp_import_name_offset(const VMGPImport *imp)
{
  if (!imp)
  {
    return 0;
  }

  return ((uint32_t)imp->name_off_0) |
         ((uint32_t)imp->name_off_1 << 8) |
         ((uint32_t)imp->name_off_2 << 16);
}

const char *vmgp_get_import_name(const VMGPContext *ctx, size_t import_index)
{
  const VMGPImport *imp;

  if (!ctx)
  {
    return NULL;
  }

  imp = vmgp_get_import(ctx, import_index);
  if (!imp || ctx->import_strtab_size == 0)
  {
    return NULL;
  }

  {
    uint32_t off = vmgp_import_name_offset(imp);
    if (ctx->import_strtab_offset + off >= ctx->size)
    {
      return NULL;
    }

    return (const char *)(ctx->data + ctx->import_strtab_offset + off);
  }
}

size_t vmgp_pool_size_bytes(const VMGPHeader *header)
{
  if (!header)
  {
    return 0;
  }

  return (size_t)header->pool_slots * VMGP_POOL_SLOT_SIZE;
}

void vmgp_dump_summary(const VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  printf("=== VMGP summary ===\n");

  if (ctx->header_valid)
  {
    printf("magic             : %.4s\n", ctx->header.magic);
    printf("field_04          : 0x%08X\n", ctx->header.field_04);
    printf("header_size       : 0x%08X\n", ctx->header.header_size);
    printf("code_size         : %u (0x%08X)\n", ctx->header.code_size, ctx->header.code_size);
    printf("data_size         : %u (0x%08X)\n", ctx->header.data_size, ctx->header.data_size);
    printf("bss_size          : %u (0x%08X)\n", ctx->header.bss_size, ctx->header.bss_size);
    printf("resource_size     : %u (0x%08X)\n", ctx->header.res_size, ctx->header.res_size);
    printf("reserved0         : 0x%08X\n", ctx->header.reserved0);
    printf("pool_slots        : %u\n", ctx->header.pool_slots);
    printf("pool_size_bytes   : %llu (0x%llX)\n",
           (unsigned long long)vmgp_pool_size_bytes(&ctx->header),
           (unsigned long long)vmgp_pool_size_bytes(&ctx->header));
    printf("string_table_size : %u (0x%08X)\n", ctx->header.string_size, ctx->header.string_size);
    printf("code_offset       : 0x%llX\n", (unsigned long long)ctx->code_offset);
    printf("code_end_offset   : 0x%llX\n", (unsigned long long)ctx->code_end_offset);
  }
  else
  {
    printf("header            : invalid\n");
  }

  if (ctx->section_tag_count > 0)
  {
    printf("section_tags      : offset=0x%llX count=%llu\n",
           (unsigned long long)ctx->section_tags_offset,
           (unsigned long long)ctx->section_tag_count);

    for (size_t i = 0; i < ctx->section_tag_count; i++)
    {
      const SectionTagRecord *rec = vmgp_get_section_tag(ctx, i);
      if (!rec)
      {
        continue;
      }

      {
        char tag[4] = {rec->tag[0], rec->tag[1], rec->tag[2], '\0'};
        printf("  [%02llu] tag=%s type=0x%04X reserved=0x%04X\n",
               (unsigned long long)i,
               tag,
               rec->type_code,
               rec->reserved);
      }
    }
  }
  else
  {
    printf("section_tags      : not found\n");
  }

  if (ctx->import_count > 0)
  {
    printf("imports           : offset=0x%llX count=%llu\n",
           (unsigned long long)ctx->imports_offset,
           (unsigned long long)ctx->import_count);
    printf("import_strtab     : offset=0x%llX size=0x%llX\n",
           (unsigned long long)ctx->import_strtab_offset,
           (unsigned long long)ctx->import_strtab_size);

    for (size_t i = 0; i < ctx->import_count; i++)
    {
      const VMGPImport *imp = vmgp_get_import(ctx, i);
      const char *name = vmgp_get_import_name(ctx, i);

      if (!imp || !name)
      {
        continue;
      }

      printf("  [%02llu] kind=0x%02X name_off=0x%06X resolved=0x%08X name=%s\n",
             (unsigned long long)i,
             imp->kind,
             vmgp_import_name_offset(imp),
             imp->resolved,
             name);
    }
  }
  else
  {
    printf("imports           : not found\n");
  }
}
