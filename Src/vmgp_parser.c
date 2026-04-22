#include "vmgp_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VM_STACK_EXTRA (64u * 1024u)
#define VM_HEAP_EXTRA (128u * 1024u)

enum
{
  OP_NOP = 0x01,
  OP_ADD = 0x02,
  OP_AND = 0x03,
  OP_MUL = 0x04,
  OP_DIVU = 0x06,
  OP_OR = 0x07,
  OP_XOR = 0x08,
  OP_SUB = 0x09,
  OP_NOT = 0x0D,
  OP_NEG = 0x0E,
  OP_EXSB = 0x0F,
  OP_EXSH = 0x10,
  OP_MOV = 0x11,
  OP_ADDB = 0x12,
  OP_SUBB = 0x13,
  OP_ANDB = 0x14,
  OP_ORB = 0x15,
  OP_MOVB = 0x16,
  OP_ADDH = 0x17,
  OP_SUBH = 0x18,
  OP_ANDH = 0x19,
  OP_ORH = 0x1A,
  OP_MOVH = 0x1B,
  OP_SLLI = 0x1C,
  OP_SRAI = 0x1D,
  OP_SRLI = 0x1E,
  OP_ADDQ = 0x1F,
  OP_MULQ = 0x20,
  OP_ADDBI = 0x21,
  OP_ANDBI = 0x22,
  OP_ORBI = 0x23,
  OP_SLLB = 0x24,
  OP_SRLB = 0x25,
  OP_SRAB = 0x26,
  OP_ADDHI = 0x27,
  OP_ANDHI = 0x28,
  OP_SLLH = 0x29,
  OP_SRLH = 0x2A,
  OP_SRAH = 0x2B,
  OP_BEQI = 0x2C,
  OP_BNEI = 0x2D,
  OP_BGEI = 0x2E,
  OP_BGTI = 0x30,
  OP_BGTUI = 0x31,
  OP_BLEI = 0x32,
  OP_BLEUI = 0x33,
  OP_BLTI = 0x34,
  OP_BEQIB = 0x36,
  OP_BNEIB = 0x37,
  OP_BGEIB = 0x38,
  OP_BGEUIB = 0x39,
  OP_BGTIB = 0x3A,
  OP_BGTUIB = 0x3B,
  OP_BLEIB = 0x3C,
  OP_BLEUIB = 0x3D,
  OP_BLTIB = 0x3E,
  OP_BLTUIB = 0x3F,
  OP_LDQ = 0x40,
  OP_JPR = 0x41,
  OP_CALLR = 0x42,
  OP_STORE = 0x43,
  OP_RESTORE = 0x44,
  OP_RET = 0x45,
  OP_SLEEP = 0x47,
  OP_SYSCPY = 0x48,
  OP_SYSSET = 0x49,
  OP_ADDI = 0x4A,
  OP_ANDI = 0x4B,
  OP_MULI = 0x4C,
  OP_DIVI = 0x4D,
  OP_DIVUI = 0x4E,
  OP_ORI = 0x4F,
  OP_STBD = 0x52,
  OP_STHD = 0x53,
  OP_STWD = 0x54,
  OP_LDBD = 0x55,
  OP_LDWD = 0x57,
  OP_LDBU = 0x58,
  OP_LDHU = 0x59,
  OP_LDI = 0x5A,
  OP_JPL = 0x5B,
  OP_CALLL = 0x5C,
  OP_BEQ = 0x5D,
  OP_BNE = 0x5E,
  OP_BGE = 0x5F,
  OP_BGTU = 0x62,
  OP_BLE = 0x63,
  OP_BLEU = 0x64,
  OP_BLT = 0x65,
  OP_BLTU = 0x66,
  OP_SYSCALL4 = 0x67,
  OP_SYSCALL0 = 0x68,
  OP_SYSCALL1 = 0x69,
  OP_SYSCALL2 = 0x6A,
  OP_SYSCALL3 = 0x6B,
};

static uint16_t read_u16_le(const uint8_t *p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p)
{
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void write_u16_le(uint8_t *p, uint16_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static uint32_t reg_index(uint8_t encoded)
{
  return (uint32_t)(encoded >> 2);
}

static uint32_t imm24_u(uint32_t v)
{
  return v & 0x00FFFFFFu;
}

static int32_t sext24(uint32_t v)
{
  v &= 0x00FFFFFFu;
  if (v & 0x00800000u)
  {
    v |= 0xFF000000u;
  }
  return (int32_t)v;
}

static int32_t reg_s32(uint32_t v)
{
  return (int32_t)v;
}

static uint32_t align4(uint32_t v)
{
  return (v + 3u) & ~3u;
}

static bool fetch_code_u32(const VMGPContext *ctx, uint32_t pc, uint32_t *out)
{
  if (!ctx || !out || pc + 4 > ctx->header.code_size)
  {
    return false;
  }
  *out = read_u32_le(ctx->data + ctx->code_file_offset + pc);
  return true;
}

static const char *opcode_name(uint8_t op)
{
  switch (op)
  {
  case OP_NOP:
    return "NOP";
  case OP_ADD:
    return "ADD";
  case OP_AND:
    return "AND";
  case OP_MUL:
    return "MUL";
  case OP_DIVU:
    return "DIVU";
  case OP_OR:
    return "OR";
  case OP_XOR:
    return "XOR";
  case OP_SUB:
    return "SUB";
  case OP_NOT:
    return "NOT";
  case OP_NEG:
    return "NEG";
  case OP_EXSB:
    return "EXSB";
  case OP_EXSH:
    return "EXSH";
  case OP_MOV:
    return "MOV";
  case OP_ADDB:
    return "ADDB";
  case OP_SUBB:
    return "SUBB";
  case OP_ANDB:
    return "ANDB";
  case OP_ORB:
    return "ORB";
  case OP_MOVB:
    return "MOVB";
  case OP_ADDH:
    return "ADDH";
  case OP_SUBH:
    return "SUBH";
  case OP_ANDH:
    return "ANDH";
  case OP_ORH:
    return "ORH";
  case OP_MOVH:
    return "MOVH";
  case OP_SLLI:
    return "SLLi";
  case OP_SRAI:
    return "SRAi";
  case OP_SRLI:
    return "SRLi";
  case OP_ADDQ:
    return "ADDQ";
  case OP_MULQ:
    return "MULQ";
  case OP_ADDBI:
    return "ADDBi";
  case OP_ANDBI:
    return "ANDBi";
  case OP_ORBI:
    return "ORBi";
  case OP_SLLB:
    return "SLLB";
  case OP_SRLB:
    return "SRLB";
  case OP_SRAB:
    return "SRAB";
  case OP_ADDHI:
    return "ADDHi";
  case OP_ANDHI:
    return "ANDHi";
  case OP_SLLH:
    return "SLLH";
  case OP_SRLH:
    return "SRLH";
  case OP_SRAH:
    return "SRAH";
  case OP_BEQI:
    return "BEQI";
  case OP_BNEI:
    return "BNEI";
  case OP_BGEI:
    return "BGEI";
  case OP_BGTI:
    return "BGTI";
  case OP_BGTUI:
    return "BGTUI";
  case OP_BLEI:
    return "BLEI";
  case OP_BLEUI:
    return "BLEUI";
  case OP_BLTI:
    return "BLTI";
  case OP_BEQIB:
    return "BEQIB";
  case OP_BNEIB:
    return "BNEIB";
  case OP_BGEIB:
    return "BGEIB";
  case OP_BGEUIB:
    return "BGEUIB";
  case OP_BGTIB:
    return "BGTIB";
  case OP_BGTUIB:
    return "BGTUIB";
  case OP_BLEIB:
    return "BLEIB";
  case OP_BLEUIB:
    return "BLEUIB";
  case OP_BLTIB:
    return "BLTIB";
  case OP_BLTUIB:
    return "BLTUIB";
  case OP_LDQ:
    return "LDQ";
  case OP_JPR:
    return "JPr";
  case OP_CALLR:
    return "CALLr";
  case OP_STORE:
    return "STORE";
  case OP_RESTORE:
    return "RESTORE";
  case OP_RET:
    return "RET";
  case OP_SLEEP:
    return "SLEEP";
  case OP_SYSCPY:
    return "SYSCPY";
  case OP_SYSSET:
    return "SYSSET";
  case OP_ADDI:
    return "ADDi";
  case OP_ANDI:
    return "ANDi";
  case OP_MULI:
    return "MULi";
  case OP_DIVI:
    return "DIVi";
  case OP_DIVUI:
    return "DIVUi";
  case OP_ORI:
    return "ORi";
  case OP_STBD:
    return "STBd";
  case OP_STHD:
    return "STHd";
  case OP_STWD:
    return "STWd";
  case OP_LDBD:
    return "LDBd";
  case OP_LDWD:
    return "LDWd";
  case OP_LDBU:
    return "LDBU";
  case OP_LDHU:
    return "LDHU";
  case OP_LDI:
    return "LDI";
  case OP_JPL:
    return "JPl";
  case OP_CALLL:
    return "CALLl";
  case OP_BEQ:
    return "BEQ";
  case OP_BNE:
    return "BNE";
  case OP_BGE:
    return "BGE";
  case OP_BGTU:
    return "BGTU";
  case OP_BLE:
    return "BLE";
  case OP_BLEU:
    return "BLEU";
  case OP_BLT:
    return "BLT";
  case OP_BLTU:
    return "BLTU";
  case OP_SYSCALL4:
    return "SYSCALL4";
  case OP_SYSCALL0:
    return "SYSCALL0";
  case OP_SYSCALL1:
    return "SYSCALL1";
  case OP_SYSCALL2:
    return "SYSCALL2";
  case OP_SYSCALL3:
    return "SYSCALL3";
  default:
    return "UNKNOWN";
  }
}

const char *vmgp_pool_type_name(uint8_t type)
{
  switch (type)
  {
  case 0x00:
    return "null";
  case 0x02:
    return "import";
  case 0x11:
    return "code";
  case 0x13:
    return "export";
  case 0x18:
    return "bytes";
  case 0x21:
    return "u32";
  case 0x23:
    return "const?";
  case 0x24:
    return "str?";
  case 0x25:
    return "res?";
  case 0x26:
    return "ptr?";
  case 0x41:
    return "addr?";
  case 0x67:
    return "sys?";
  default:
    return "unknown";
  }
}

size_t vmgp_pool_size_bytes(const VMGPHeader *header)
{
  return header ? (size_t)header->pool_slots * VMGP_POOL_SLOT_SIZE : 0u;
}

const VMGPPoolEntry *vmgp_get_pool_entry(const VMGPContext *ctx, uint32_t pool_index_1based)
{
  if (!ctx || !ctx->pool || pool_index_1based == 0 || pool_index_1based > ctx->header.pool_slots)
  {
    return NULL;
  }
  return &ctx->pool[pool_index_1based - 1u];
}

static uint32_t vmgp_resolve_pool_value(const VMGPContext *ctx, const VMGPPoolEntry *entry)
{
  if (!ctx || !entry)
  {
    return 0;
  }

  switch (entry->type)
  {
  case 0x21: /* .data */
  case 0x23: /* global .data */
    return ctx->data_offset + entry->value;
  case 0x41: /* .bss */
    return ctx->bss_offset + entry->value;
  case 0x11: /* .text */
  case 0x67: /* absolute const */
  default:
    return entry->value;
  }
}

static bool vmgp_load_resources(VMGPContext *ctx)
{
  uint32_t prev = 0;
  uint32_t count = 0;
  uint32_t i;

  if (!ctx || ctx->header.res_size < 8)
  {
    return true;
  }

  for (i = 0; i + 4 <= ctx->header.res_size; i += 4)
  {
    uint32_t off = read_u32_le(ctx->data + ctx->res_file_offset + i);
    if (off == 0)
    {
      break;
    }
    if (off >= ctx->header.res_size || off < prev)
    {
      break;
    }
    prev = off;
    count++;
  }

  if (count == 0)
  {
    return true;
  }

  ctx->resources = (VMGPResource *)calloc(count, sizeof(VMGPResource));
  if (!ctx->resources)
  {
    return false;
  }
  ctx->resource_count = count;

  for (i = 0; i < count; ++i)
  {
    uint32_t off = read_u32_le(ctx->data + ctx->res_file_offset + i * 4u);
    uint32_t next = (i + 1u < count)
                        ? read_u32_le(ctx->data + ctx->res_file_offset + (i + 1u) * 4u)
                        : ctx->header.res_size;
    ctx->resources[i].id = i + 1u;
    ctx->resources[i].offset = off;
    ctx->resources[i].size = (next > off) ? (next - off) : 0u;
  }
  return true;
}

static bool vmgp_build_vm_memory(VMGPContext *ctx)
{
  ctx->vm_end = ctx->res_offset + ctx->header.res_size;
  ctx->heap_base = align4(ctx->vm_end);
  ctx->heap_cur = ctx->heap_base;
  ctx->heap_limit = ctx->heap_base + VM_HEAP_EXTRA;
  ctx->stack_top = ctx->heap_limit + VM_STACK_EXTRA;
  ctx->mem_size = ctx->stack_top + 0x100u;

  ctx->mem = (uint8_t *)calloc(ctx->mem_size, 1);
  if (!ctx->mem)
  {
    return false;
  }

  memcpy(ctx->mem + ctx->data_offset, ctx->data + ctx->data_file_offset, ctx->header.data_size);
  if (ctx->header.res_size > 0)
  {
    memcpy(ctx->mem + ctx->res_offset, ctx->data + ctx->res_file_offset, ctx->header.res_size);
  }

  ctx->pc = 0;
  ctx->regs[VM_REG_SP] = ctx->stack_top;
  ctx->regs[VM_REG_ZERO] = 0;
  return true;
}

static const char *vm_file_str(const VMGPContext *ctx, uint32_t off)
{
  if (!ctx || ctx->strtab_offset + off >= ctx->size)
  {
    return NULL;
  }
  return (const char *)(ctx->data + ctx->strtab_offset + off);
}

const char *vmgp_get_import_name(const VMGPContext *ctx, uint32_t pool_index_1based)
{
  static char bad[32];
  const VMGPPoolEntry *entry = vmgp_get_pool_entry(ctx, pool_index_1based);
  const char *name;

  if (!entry)
  {
    snprintf(bad, sizeof(bad), "<bad:%u>", pool_index_1based);
    return bad;
  }
  if (entry->type != 0x02)
  {
    snprintf(bad, sizeof(bad), "<type:%02X>", entry->type);
    return bad;
  }

  name = vm_file_str(ctx, entry->aux24);
  return name ? name : "<str-oob>";
}

static const VMGPResource *vmgp_get_resource(const VMGPContext *ctx, uint32_t resource_id)
{
  uint32_t i;

  if (!ctx || !ctx->resources || resource_id == 0)
  {
    return NULL;
  }

  for (i = 0; i < ctx->resource_count; ++i)
  {
    if (ctx->resources[i].id == resource_id)
    {
      return &ctx->resources[i];
    }
  }
  return NULL;
}

bool vmgp_init(VMGPContext *ctx, const uint8_t *data, size_t size)
{
  if (!ctx || !data || size < sizeof(VMGPHeader))
  {
    return false;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->data = data;
  ctx->size = size;
  ctx->next_stream_handle = 0x30u;
  ctx->random_state = 1u;
  return true;
}

void vmgp_free(VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }
  free(ctx->pool);
  free(ctx->resources);
  free(ctx->mem);
  memset(ctx, 0, sizeof(*ctx));
}

bool vmgp_parse_header(VMGPContext *ctx)
{
  if (!ctx || !ctx->data || ctx->size < sizeof(VMGPHeader))
  {
    return false;
  }

  memcpy(ctx->header.magic, ctx->data + 0x00, 4);
  ctx->header.unknown1 = read_u16_le(ctx->data + 0x04);
  ctx->header.unknown2 = read_u16_le(ctx->data + 0x06);
  ctx->header.stack_words = read_u16_le(ctx->data + 0x08);
  ctx->header.unknown3 = ctx->data[0x0A];
  ctx->header.unknown4 = ctx->data[0x0B];
  ctx->header.code_size = read_u32_le(ctx->data + 0x0C);
  ctx->header.data_size = read_u32_le(ctx->data + 0x10);
  ctx->header.bss_size = read_u32_le(ctx->data + 0x14);
  ctx->header.res_size = read_u32_le(ctx->data + 0x18);
  ctx->header.unknown5 = read_u32_le(ctx->data + 0x1C);
  ctx->header.pool_slots = read_u32_le(ctx->data + 0x20);
  ctx->header.string_size = read_u32_le(ctx->data + 0x24);

  ctx->header_valid = (memcmp(ctx->header.magic, VMGP_MAGIC, 4) == 0);
  if (!ctx->header_valid)
  {
    return false;
  }

  ctx->code_file_offset = (uint32_t)sizeof(VMGPHeader);
  ctx->data_file_offset = ctx->code_file_offset + ctx->header.code_size;
  ctx->code_offset = 0;
  ctx->data_offset = 0;
  ctx->bss_offset = ctx->data_offset + ctx->header.data_size;
  ctx->res_offset = ctx->bss_offset + ctx->header.bss_size;
  ctx->res_file_offset = ctx->data_file_offset + ctx->header.data_size;
  ctx->pool_offset = ctx->res_file_offset + ctx->header.res_size;
  ctx->strtab_offset = ctx->pool_offset + ctx->header.pool_slots * VMGP_POOL_SLOT_SIZE;

  if ((size_t)ctx->strtab_offset + ctx->header.string_size > ctx->size)
  {
    return false;
  }

  return true;
}

bool vmgp_load_pool(VMGPContext *ctx)
{
  uint32_t i;

  if (!ctx || !ctx->header_valid)
  {
    return false;
  }

  ctx->pool = (VMGPPoolEntry *)calloc(ctx->header.pool_slots, sizeof(VMGPPoolEntry));
  if (!ctx->pool)
  {
    return false;
  }

  for (i = 0; i < ctx->header.pool_slots; ++i)
  {
    uint32_t off = ctx->pool_offset + i * VMGP_POOL_SLOT_SIZE;
    ctx->pool[i].type = ctx->data[off + 0];
    ctx->pool[i].aux24 = (uint32_t)ctx->data[off + 1] |
                         ((uint32_t)ctx->data[off + 2] << 8) |
                         ((uint32_t)ctx->data[off + 3] << 16);
    ctx->pool[i].value = read_u32_le(ctx->data + off + 4);
  }

  if (!vmgp_load_resources(ctx))
  {
    return false;
  }
  if (!vmgp_build_vm_memory(ctx))
  {
    return false;
  }
  return true;
}

void vmgp_dump_summary(const VMGPContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  printf("=== VMGP summary ===\n");
  printf("magic             : %.4s\n", ctx->header.magic);
  printf("unknown1          : 0x%04X\n", ctx->header.unknown1);
  printf("unknown2          : 0x%04X\n", ctx->header.unknown2);
  printf("stack_words       : %u (0x%X)\n", ctx->header.stack_words, ctx->header.stack_words);
  printf("code_size         : %u (0x%X)\n", ctx->header.code_size, ctx->header.code_size);
  printf("data_size         : %u (0x%X)\n", ctx->header.data_size, ctx->header.data_size);
  printf("bss_size          : %u (0x%X)\n", ctx->header.bss_size, ctx->header.bss_size);
  printf("res_size          : %u (0x%X)\n", ctx->header.res_size, ctx->header.res_size);
  printf("pool_slots        : %u\n", ctx->header.pool_slots);
  printf("string_table_size : %u (0x%X)\n", ctx->header.string_size, ctx->header.string_size);
  printf("code_offset(vm)   : 0x%X\n", ctx->code_offset);
  printf("code_offset(file) : 0x%X\n", ctx->code_file_offset);
  printf("data_offset       : 0x%X\n", ctx->data_offset);
  printf("bss_offset        : 0x%X\n", ctx->bss_offset);
  printf("res_offset        : 0x%X\n", ctx->res_offset);
  printf("pool_offset(file) : 0x%X\n", ctx->pool_offset);
  printf("strtab_offset     : 0x%X\n", ctx->strtab_offset);
  printf("vm_end            : 0x%X\n", ctx->vm_end);
  printf("heap_base         : 0x%X\n", ctx->heap_base);
  printf("heap_limit        : 0x%X\n", ctx->heap_limit);
  printf("stack_top         : 0x%X\n", ctx->stack_top);
  printf("resource_count    : %u\n", ctx->resource_count);
}

void vmgp_dump_imports(const VMGPContext *ctx, uint32_t max_count)
{
  uint32_t i;
  if (!ctx || !ctx->pool)
  {
    return;
  }
  printf("=== imports (leading type=0x02 pool entries) ===\n");
  for (i = 1; i <= ctx->header.pool_slots && i <= max_count; ++i)
  {
    const VMGPPoolEntry *e = &ctx->pool[i - 1];
    if (e->type != 0x02)
    {
      break;
    }
    printf("[%03u] %s\n", i, vmgp_get_import_name(ctx, i));
  }
}

static uint32_t stack_arg0(const VMGPContext *ctx)
{
  return (ctx->regs[VM_REG_SP] + 4 <= ctx->mem_size) ? read_u32_le(ctx->mem + ctx->regs[VM_REG_SP]) : 0u;
}

static void log_vm_call(VMGPContext *ctx, uint32_t call_site, uint32_t index, const VMGPPoolEntry *e)
{
  if (e->type == 0x02)
  {
    printf("[vm-call %02u] pc=0x%08X CALLl pool[%u] import=%s sp=%08X stk0=%08X p0=%08X p1=%08X p2=%08X p3=%08X r0=%08X\n",
           ctx->logged_calls + 1,
           call_site,
           index,
           vmgp_get_import_name(ctx, index),
           ctx->regs[VM_REG_SP],
           stack_arg0(ctx),
           ctx->regs[VM_REG_P0],
           ctx->regs[VM_REG_P1],
           ctx->regs[VM_REG_P2],
           ctx->regs[VM_REG_P3],
           ctx->regs[VM_REG_R0]);
  }
  else
  {
    printf("[vm-call %02u] pc=0x%08X CALLl pool[%u] type=0x%02X(%s) value=0x%08X aux=0x%06X\n",
           ctx->logged_calls + 1,
           call_site,
           index,
           e->type,
           vmgp_pool_type_name(e->type),
           e->value,
           e->aux24);
  }
  ctx->logged_calls++;
}

static void log_syscall(VMGPContext *ctx, uint8_t op)
{
  uint32_t argc = (op == OP_SYSCALL0) ? 0u : (uint32_t)(op - OP_SYSCALL0);
  printf("[syscall %02u] pc=0x%08X %s argc=%u p0=%08X p1=%08X p2=%08X p3=%08X\n",
         ctx->logged_calls + 1,
         ctx->pc,
         opcode_name(op),
         argc,
         ctx->regs[VM_REG_P0],
         ctx->regs[VM_REG_P1],
         ctx->regs[VM_REG_P2],
         ctx->regs[VM_REG_P3]);
  ctx->logged_calls++;
}

static VMGPStream *find_stream(VMGPContext *ctx, uint32_t handle)
{
  uint32_t i;
  for (i = 0; i < VMGP_MAX_STREAMS; ++i)
  {
    if (ctx->streams[i].used && ctx->streams[i].handle == handle)
    {
      return &ctx->streams[i];
    }
  }
  return NULL;
}

static VMGPStream *alloc_stream(VMGPContext *ctx)
{
  uint32_t i;
  for (i = 0; i < VMGP_MAX_STREAMS; ++i)
  {
    if (!ctx->streams[i].used)
    {
      memset(&ctx->streams[i], 0, sizeof(ctx->streams[i]));
      ctx->streams[i].used = true;
      ctx->streams[i].handle = i;
      return &ctx->streams[i];
    }
  }
  return NULL;
}

static uint32_t vm_strlen(const uint8_t *s, size_t max_len)
{
  uint32_t n = 0;
  while (n < max_len && s[n] != 0)
  {
    ++n;
  }
  return n;
}

static bool lz_read_header(const uint8_t *p,
                           size_t remain,
                           uint8_t *extended_offset_bits,
                           uint8_t *max_offset_bits,
                           uint32_t *uncompressed_size,
                           uint32_t *compressed_size)
{
  uint32_t raw_size;
  uint32_t packed_size;

  if (!p || remain < 22 || p[0] != 'L' || p[1] != 'Z')
  {
    return false;
  }

  raw_size = read_u32_le(p + 4);
  packed_size = read_u32_le(p + 8);

  if (extended_offset_bits)
    *extended_offset_bits = p[3];
  if (max_offset_bits)
    *max_offset_bits = p[2];
  if (compressed_size)
    *compressed_size = packed_size;

  /* Observed on this T310 game: these entries allocate packed_size - 1. */
  if (raw_size == 0x200u && packed_size > 1u && packed_size < raw_size)
  {
    raw_size = packed_size - 1u;
  }

  if (uncompressed_size)
  {
    *uncompressed_size = raw_size;
  }

  return true;
}

typedef struct LZBitStream
{
  const uint8_t *data;
  uint32_t size;
  uint32_t bit_pos;
} LZBitStream;

static bool lz_bits_valid(const LZBitStream *bs)
{
  return bs && bs->bit_pos < bs->size * 8u;
}

static uint32_t lz_read_bits(LZBitStream *bs, uint32_t count)
{
  uint32_t result = 0;
  uint32_t i;

  for (i = 0; i < count; ++i)
  {
    result <<= 1;
    if (lz_bits_valid(bs))
    {
      uint32_t byte_index = bs->bit_pos >> 3;
      uint32_t bit_index = 7u - (bs->bit_pos & 7u);
      result |= (uint32_t)((bs->data[byte_index] >> bit_index) & 1u);
      bs->bit_pos++;
    }
  }

  return result;
}

static uint32_t lz_decompress_content(const uint8_t *src,
                                      uint32_t src_size,
                                      uint8_t *dst,
                                      uint32_t dst_size,
                                      uint8_t extended_offset_bits,
                                      uint8_t max_offset_bits)
{
  LZBitStream bs;
  uint32_t dst_pos = 0;

  bs.data = src;
  bs.size = src_size;
  bs.bit_pos = 0;

  while (dst_pos < dst_size && lz_bits_valid(&bs))
  {
    if (lz_read_bits(&bs, 1) == 1)
    {
      uint32_t v2 = 0;
      uint32_t copy_len = 2;
      uint32_t back_offset;
      uint32_t i;

      while (v2 < max_offset_bits && lz_read_bits(&bs, 1) == 1)
      {
        v2++;
      }

      if (v2 != 0)
      {
        copy_len = (lz_read_bits(&bs, v2) | (1u << v2)) + 1u;
      }

      if (copy_len == 2)
      {
        back_offset = lz_read_bits(&bs, 8) + 2u;
      }
      else
      {
        back_offset = lz_read_bits(&bs, extended_offset_bits) + copy_len;
      }

      for (i = 0; i < copy_len && dst_pos < dst_size; ++i)
      {
        uint32_t from = (back_offset <= dst_pos) ? (dst_pos - back_offset) : 0u;
        dst[dst_pos] = dst[from];
        dst_pos++;
      }
    }
    else
    {
      dst[dst_pos++] = (uint8_t)(lz_read_bits(&bs, 8) & 0xFFu);
    }
  }

  return dst_pos;
}

static bool mem_range_ok(const VMGPContext *ctx, uint32_t addr, uint32_t size)
{
  return ctx && addr <= ctx->mem_size && size <= ctx->mem_size - addr;
}

static void write_watch_if_code(const VMGPContext *ctx, uint32_t addr, uint32_t size, const char *tag)
{
  (void)ctx;
  (void)addr;
  (void)size;
  (void)tag;
}

static bool handle_import_call(VMGPContext *ctx, uint32_t pool_index)
{
  const char *name = vmgp_get_import_name(ctx, pool_index);

  if (strcmp(name, "vStreamOpen") == 0)
  {
    uint32_t mode = ctx->regs[VM_REG_P1];
    uint32_t resid = mode >> 16;
    VMGPStream *s = alloc_stream(ctx);
    if (!s)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    if (resid != 0)
    {
      const VMGPResource *res = vmgp_get_resource(ctx, resid);
      if (!res)
      {
        memset(s, 0, sizeof(*s));
        ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
        return true;
      }
      s->base = ctx->res_offset + res->offset;
      s->size = res->size;
      s->resource_id = resid;
    }
    else
    {
      s->base = ctx->res_offset;
      s->size = ctx->header.res_size;
    }
    s->pos = 0;
    ctx->regs[VM_REG_R0] = s->handle;
    return true;
  }

  if (strcmp(name, "vStreamSeek") == 0)
  {
    VMGPStream *s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    int32_t where = reg_s32(ctx->regs[VM_REG_P1]);
    uint32_t whence = ctx->regs[VM_REG_P2];
    int32_t pos = -1;
    if (!s)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    if (whence == 0)
      pos = where;
    else if (whence == 1)
      pos = (int32_t)s->pos + where;
    else if (whence == 2)
      pos = (int32_t)s->size + where;
    if (pos < 0)
      pos = 0;
    if ((uint32_t)pos > s->size)
      pos = (int32_t)s->size;
    s->pos = (uint32_t)pos;
    ctx->regs[VM_REG_R0] = s->pos;
    return true;
  }

  if (strcmp(name, "vStreamRead") == 0)
  {
    VMGPStream *s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    uint32_t buf = ctx->regs[VM_REG_P1];
    uint32_t count = ctx->regs[VM_REG_P2];
    uint32_t avail;
    if (!s || buf >= ctx->mem_size)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    avail = (s->pos < s->size) ? (s->size - s->pos) : 0u;
    if (count > avail)
      count = avail;
    if ((size_t)buf + count > ctx->mem_size)
      count = (uint32_t)(ctx->mem_size - buf);
    if ((size_t)s->base + s->pos + count > ctx->mem_size)
      count = 0;
    write_watch_if_code(ctx, buf, count, "vStreamRead");
    memcpy(ctx->mem + buf, ctx->mem + s->base + s->pos, count);
    s->pos += count;
    ctx->regs[VM_REG_R0] = count;
    return true;
  }

  if (strcmp(name, "vStreamClose") == 0)
  {
    VMGPStream *s = find_stream(ctx, ctx->regs[VM_REG_P0]);
    if (s)
      memset(s, 0, sizeof(*s));
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  if (strcmp(name, "vGetCaps") == 0)
  {
    uint32_t query = ctx->regs[VM_REG_P0];
    uint32_t out = ctx->regs[VM_REG_P1];

    if (query == 0 && mem_range_ok(ctx, out, 8))
    {
      write_u16_le(ctx->mem + out + 0, 8);      /* sizeof(VideoCaps) */
      write_u16_le(ctx->mem + out + 2, 8);      /* T310-style color depth/mode */
      write_u16_le(ctx->mem + out + 4, 101);
      write_u16_le(ctx->mem + out + 6, 80);
      ctx->regs[VM_REG_R0] = 1;
      return true;
    }

    if (query == 2 && mem_range_ok(ctx, out, 4))
    {
      write_u16_le(ctx->mem + out + 0, 4);    /* legacy SoundCaps */
      write_u16_le(ctx->mem + out + 2, 0x000F);
      ctx->regs[VM_REG_R0] = 1;
      return true;
    }

    if (query == 3 && mem_range_ok(ctx, out, 4))
    {
      write_u16_le(ctx->mem + out + 0, 4);      /* sizeof(CommCaps) */
      write_u16_le(ctx->mem + out + 2, 0x00A7); /* file/tcp/udp/sms/http */
      ctx->regs[VM_REG_R0] = 1;
      return true;
    }

    if (query == 4 && mem_range_ok(ctx, out, 12))
    {
      write_u16_le(ctx->mem + out + 0, 12);   /* sizeof(SystemCaps) */
      write_u16_le(ctx->mem + out + 2, 0x25); /* unicode + vibrate + openurl */
      write_u32_le(ctx->mem + out + 4, (1u << 16) | 3u); /* T310, SonyEricsson */
      write_u32_le(ctx->mem + out + 8, 0);
      ctx->regs[VM_REG_R0] = 1;
      return true;
    }

    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  if (strcmp(name, "vDecompHdr") == 0)
  {
    uint32_t info = ctx->regs[VM_REG_P0];
    uint32_t hdr = ctx->regs[VM_REG_P1];
    uint8_t extended_offset_bits = 0;
    uint8_t max_offset_bits = 0;
    uint32_t uncompressed_size = 0;
    uint32_t compressed_size = 0;

    if (!mem_range_ok(ctx, hdr, 22) ||
        !lz_read_header(ctx->mem + hdr,
                        ctx->mem_size - hdr,
                        &extended_offset_bits,
                        &max_offset_bits,
                        &uncompressed_size,
                        &compressed_size))
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }

    if (info != 0 && mem_range_ok(ctx, info, 20))
    {
      ctx->mem[info + 0] = 0;
      ctx->mem[info + 1] = 0;
      write_u16_le(ctx->mem + info + 2, 0x1234);
      write_u16_le(ctx->mem + info + 4, 0);
      write_u16_le(ctx->mem + info + 6, 0);
      write_u32_le(ctx->mem + info + 8, compressed_size);
      write_u32_le(ctx->mem + info + 12, uncompressed_size);
      write_u32_le(ctx->mem + info + 16, 0);
    }

    ctx->regs[VM_REG_R0] = uncompressed_size;
    return true;
  }

  if (strcmp(name, "vDecompress") == 0)
  {
    uint32_t src = ctx->regs[VM_REG_P0];
    uint32_t dst = ctx->regs[VM_REG_P1];
    uint32_t stream_handle = ctx->regs[VM_REG_P2];
    VMGPStream *s = NULL;
    const uint8_t *base = NULL;
    uint32_t available = 0;
    uint32_t stream_base_pos = 0;
    uint8_t extended_offset_bits = 0;
    uint8_t max_offset_bits = 0;
    uint32_t out_size = 0;
    uint32_t packed_size = 0;
    uint32_t produced = 0;

    if (src != 0)
    {
      if (!mem_range_ok(ctx, src, 22))
      {
        ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
        return true;
      }
      base = ctx->mem + src;
      available = (uint32_t)(ctx->mem_size - src);
    }
    else
    {
      s = find_stream(ctx, stream_handle);
      if (!s || !mem_range_ok(ctx, s->base + s->pos, 22))
      {
        ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
        return true;
      }
      stream_base_pos = s->pos;
      base = ctx->mem + s->base + s->pos;
      available = s->size - s->pos;
    }

    if (!lz_read_header(base, available, &extended_offset_bits, &max_offset_bits, &out_size, &packed_size))
    {
      uint32_t copy_size = available;
      uint32_t dst_limit;

      if (dst >= ctx->mem_size)
      {
        ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
        return true;
      }

      dst_limit = ctx->mem_size - dst;
      if (dst < ctx->heap_cur)
      {
        uint32_t heap_limit = ctx->heap_cur - dst;
        if (heap_limit < dst_limit)
          dst_limit = heap_limit;
      }

      if (copy_size > dst_limit)
        copy_size = dst_limit;

      if (copy_size > 0)
        memcpy(ctx->mem + dst, base, copy_size);

      if (s)
      {
        s->pos = stream_base_pos + copy_size;
        if (s->pos > s->size)
          s->pos = s->size;
      }

      ctx->regs[VM_REG_R0] = copy_size;
      return true;
    }

    if (!mem_range_ok(ctx, dst, out_size))
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }

    if (packed_size > available - 22u)
    {
      packed_size = available - 22u;
    }

    produced = lz_decompress_content(base + 22u,
                                     packed_size,
                                     ctx->mem + dst,
                                     out_size,
                                     extended_offset_bits,
                                     max_offset_bits);

    if (s)
    {
      uint32_t consumed = 22u + packed_size;
      s->pos = stream_base_pos + consumed;
      if (s->pos > s->size)
        s->pos = s->size;
    }

    ctx->regs[VM_REG_R0] = produced;
    return true;
  }

  if (strcmp(name, "vNewPtr") == 0)
  {
    uint32_t size = ctx->regs[VM_REG_P0];
    uint32_t addr = align4(ctx->heap_cur);
    if (size == 0)
      size = 4;
    if (addr + size > ctx->heap_limit)
      ctx->regs[VM_REG_R0] = 0;
    else
    {
      ctx->regs[VM_REG_R0] = addr;
      ctx->heap_cur = addr + size;
    }
    return true;
  }

  if (strcmp(name, "vDisposePtr") == 0 || strcmp(name, "vMemFree") == 0)
  {
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  if (strcmp(name, "vGetTickCount") == 0)
  {
    ctx->tick_count += 16u;
    ctx->regs[VM_REG_R0] = ctx->tick_count;
    return true;
  }

  if (strcmp(name, "vSetRandom") == 0)
  {
    ctx->random_state = ctx->regs[VM_REG_P0] ? ctx->regs[VM_REG_P0] : 1u;
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  if (strcmp(name, "vGetRandom") == 0)
  {
    ctx->random_state = ctx->random_state * 1103515245u + 12345u;
    ctx->regs[VM_REG_R0] = (ctx->random_state >> 16) & 0xFFFFu;
    return true;
  }

  if (strcmp(name, "vStrLen") == 0)
  {
    uint32_t p = ctx->regs[VM_REG_P0];
    ctx->regs[VM_REG_R0] = (p < ctx->mem_size) ? vm_strlen(ctx->mem + p, ctx->mem_size - p) : 0u;
    return true;
  }

  if (strcmp(name, "vStrCpy") == 0)
  {
    uint32_t dst = ctx->regs[VM_REG_P0];
    uint32_t src = ctx->regs[VM_REG_P1];
    if (dst < ctx->mem_size && src < ctx->mem_size)
    {
      size_t max_copy = ctx->mem_size - dst;
      size_t n = vm_strlen(ctx->mem + src, ctx->mem_size - src);
      if (n + 1 > max_copy)
        n = max_copy ? max_copy - 1 : 0;
      write_watch_if_code(ctx, dst, (uint32_t)(n + 1), "vStrCpy");
      memmove(ctx->mem + dst, ctx->mem + src, n);
      if (max_copy)
        ctx->mem[dst + n] = 0;
    }
    ctx->regs[VM_REG_R0] = dst;
    return true;
  }

  if (strcmp(name, "vTerminateVMGP") == 0)
  {
    /* For trace mode, just log and continue. Some startup paths call this early. */
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  if (strcmp(name, "DbgPrintf") == 0 || strcmp(name, "vPrint") == 0)
  {
    ctx->regs[VM_REG_R0] = 0;
    return true;
  }

  ctx->regs[VM_REG_R0] = 0;
  return true;
}

static bool vmgp_step(VMGPContext *ctx)
{
  uint32_t w, ext;
  uint8_t op, b1, b2, b3;
  uint32_t rd, rs, rt;

  if (ctx->halted)
  {
    return true;
  }
  if (!fetch_code_u32(ctx, ctx->pc, &w))
  {
    fprintf(stderr, "pc out of code: 0x%08X\n", ctx->pc);
    return false;
  }

  op = (uint8_t)(w & 0xFFu);
  b1 = (uint8_t)((w >> 8) & 0xFFu);
  b2 = (uint8_t)((w >> 16) & 0xFFu);
  b3 = (uint8_t)((w >> 24) & 0xFFu);
  rd = reg_index(b1);
  rs = reg_index(b2);
  rt = reg_index(b3);

  switch (op)
  {
  case OP_NOP:
  case OP_SLEEP:
    ctx->pc += 4;
    break;
  case OP_ADD:
    ctx->regs[rd] = ctx->regs[rs] + ctx->regs[rt];
    ctx->pc += 4;
    break;
  case OP_AND:
    ctx->regs[rd] = ctx->regs[rs] & ctx->regs[rt];
    ctx->pc += 4;
    break;
  case OP_MUL:
    ctx->regs[rd] = ctx->regs[rs] * ctx->regs[rt];
    ctx->pc += 4;
    break;
  case OP_DIVU:
    if (ctx->regs[rt] == 0)
    {
      fprintf(stderr, "DIVU by zero at pc=0x%X\n", ctx->pc);
      return false;
    }
    ctx->regs[rd] = ctx->regs[rs] / ctx->regs[rt];
    ctx->pc += 4;
    break;
  case OP_OR:
    ctx->regs[rd] = ctx->regs[rs] | ctx->regs[rt];
    ctx->pc += 4;
    break;
  case OP_XOR:
    ctx->regs[rd] = ctx->regs[rs] ^ ctx->regs[rt];
    ctx->pc += 4;
    break;
  case OP_SUB:
    ctx->regs[rd] = ctx->regs[rs] - ctx->regs[rt];
    ctx->pc += 4;
    break;
  case OP_NOT:
    ctx->regs[rd] = ~ctx->regs[rs];
    ctx->pc += 4;
    break;
  case OP_NEG:
    ctx->regs[rd] = (uint32_t)(-reg_s32(ctx->regs[rs]));
    ctx->pc += 4;
    break;
  case OP_EXSB:
    ctx->regs[rd] = (uint32_t)(int32_t)(int8_t)(ctx->regs[rs] & 0xFFu);
    ctx->pc += 4;
    break;
  case OP_EXSH:
    ctx->regs[rd] = (uint32_t)(int32_t)(int16_t)(ctx->regs[rs] & 0xFFFFu);
    ctx->pc += 4;
    break;
  case OP_MOV:
    ctx->regs[rd] = ctx->regs[rs];
    ctx->pc += 4;
    break;
  case OP_ADDB:
    ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) + (ctx->regs[rt] & 0xFFu)) & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_SUBB:
    ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) - (ctx->regs[rt] & 0xFFu)) & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_ANDB:
    ctx->regs[rd] = (ctx->regs[rs] & ctx->regs[rt]) & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_ORB:
    ctx->regs[rd] = (ctx->regs[rs] | ctx->regs[rt]) & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_MOVB:
    ctx->regs[rd] = ctx->regs[rs] & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_ADDH:
    ctx->regs[rd] = ((ctx->regs[rs] & 0xFFFFu) + (ctx->regs[rt] & 0xFFFFu)) & 0xFFFFu;
    ctx->pc += 4;
    break;
  case OP_SUBH:
    ctx->regs[rd] = ((ctx->regs[rs] & 0xFFFFu) - (ctx->regs[rt] & 0xFFFFu)) & 0xFFFFu;
    ctx->pc += 4;
    break;
  case OP_ANDH:
    ctx->regs[rd] = (ctx->regs[rs] & ctx->regs[rt]) & 0xFFFFu;
    ctx->pc += 4;
    break;
  case OP_ORH:
    ctx->regs[rd] = (ctx->regs[rs] | ctx->regs[rt]) & 0xFFFFu;
    ctx->pc += 4;
    break;
  case OP_MOVH:
    ctx->regs[rd] = ctx->regs[rs] & 0xFFFFu;
    ctx->pc += 4;
    break;
  case OP_SLLI:
    ctx->regs[rd] = ctx->regs[rs] << b3;
    ctx->pc += 4;
    break;
  case OP_SRAI:
    ctx->regs[rd] = (uint32_t)(reg_s32(ctx->regs[rs]) >> b3);
    ctx->pc += 4;
    break;
  case OP_SRLI:
    ctx->regs[rd] = ctx->regs[rs] >> b3;
    ctx->pc += 4;
    break;
  case OP_ADDQ:
    ctx->regs[rd] = ctx->regs[rs] + (int8_t)b3;
    ctx->pc += 4;
    break;
  case OP_MULQ:
    ctx->regs[rd] = ctx->regs[rs] * (uint32_t)((uint8_t)b3);
    ctx->pc += 4;
    break;
  case OP_ADDBI:
    ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) + b3) & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_ANDBI:
    ctx->regs[rd] = ctx->regs[rs] & (uint32_t)b3;
    ctx->pc += 4;
    break;
  case OP_ORBI:
    ctx->regs[rd] = (ctx->regs[rs] | (uint32_t)b3) & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_SLLB:
    ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) << b3) & 0xFFu;
    ctx->pc += 4;
    break;
  case OP_SRLB:
    ctx->regs[rd] = (ctx->regs[rs] & 0xFFu) >> b3;
    ctx->pc += 4;
    break;
  case OP_SRAB:
    ctx->regs[rd] = (uint32_t)((uint8_t)((int8_t)(ctx->regs[rs] & 0xFFu) >> b3));
    ctx->pc += 4;
    break;
  case OP_ADDHI:
    ctx->regs[rd] = (ctx->regs[rs] + (uint32_t)(int8_t)b3) & 0xFFFFu;
    ctx->pc += 4;
    break;
  case OP_ANDHI:
    ctx->regs[rd] = ctx->regs[rs] & (uint32_t)b3;
    ctx->pc += 4;
    break;
  case OP_SLLH:
    ctx->regs[rd] = (ctx->regs[rs] << b3) & 0xFFFFu;
    ctx->pc += 4;
    break;
  case OP_SRLH:
    ctx->regs[rd] = (ctx->regs[rs] & 0xFFFFu) >> b3;
    ctx->pc += 4;
    break;
  case OP_SRAH:
    ctx->regs[rd] = (uint32_t)((uint16_t)((int16_t)(ctx->regs[rs] & 0xFFFFu) >> b3));
    ctx->pc += 4;
    break;
  case OP_LDQ:
    ctx->regs[rd] = (uint32_t)(int32_t)(int16_t)((w >> 16) & 0xFFFFu);
    ctx->pc += 4;
    break;
  case OP_ADDI:
  case OP_ANDI:
  case OP_MULI:
  case OP_DIVI:
  case OP_DIVUI:
  case OP_ORI:
    if (!fetch_code_u32(ctx, ctx->pc + 4, &ext))
      return false;
    if (op == OP_ADDI)
      ctx->regs[rd] = ctx->regs[rs] + (uint32_t)sext24(ext);
    else if (op == OP_ANDI)
      ctx->regs[rd] = ctx->regs[rs] & (uint32_t)sext24(ext);
    else if (op == OP_MULI)
      ctx->regs[rd] = ctx->regs[rs] * (uint32_t)sext24(ext);
    else if (op == OP_ORI)
      ctx->regs[rd] = ctx->regs[rs] | (uint32_t)sext24(ext);
    else if (op == OP_DIVUI)
    {
      uint32_t immu = (uint32_t)sext24(ext);
      ctx->regs[rd] = (immu == 0) ? 0u : (ctx->regs[rs] / immu);
    }
    else
    {
      int32_t imm = sext24(ext);
      if (imm == 0)
      {
        fprintf(stderr, "DIVi by zero at pc=0x%X\n", ctx->pc);
        return false;
      }
      ctx->regs[rd] = (uint32_t)(reg_s32(ctx->regs[rs]) / imm);
    }
    ctx->pc += 8;
    break;
  case OP_LDI:
    if (!fetch_code_u32(ctx, ctx->pc + 4, &ext))
      return false;
    if ((ext >> 24) == 0x00)
    {
      const VMGPPoolEntry *entry = vmgp_get_pool_entry(ctx, imm24_u(ext));
      if (!entry)
      {
        fprintf(stderr, "LDI pool index OOB at pc=0x%X\n", ctx->pc);
        return false;
      }
      ctx->regs[rd] = vmgp_resolve_pool_value(ctx, entry);
    }
    else
    {
      ctx->regs[rd] = (uint32_t)sext24(ext);
    }
    ctx->pc += 8;
    break;
  case OP_LDWD:
  case OP_LDBD:
  case OP_LDBU:
  case OP_LDHU:
  case OP_STWD:
  case OP_STHD:
  case OP_STBD:
    if (!fetch_code_u32(ctx, ctx->pc + 4, &ext))
      return false;
    {
      const VMGPPoolEntry *entry = NULL;
      uint32_t off = ((ext >> 24) == 0x00)
                         ? ((entry = vmgp_get_pool_entry(ctx, imm24_u(ext))) ? vmgp_resolve_pool_value(ctx, entry) : 0u)
                         : (uint32_t)sext24(ext);
      uint32_t addr = ctx->regs[rs] + off;
      if ((ext >> 24) == 0x00 && !entry)
      {
        fprintf(stderr, "%s pool index OOB at pc=0x%X\n", opcode_name(op), ctx->pc);
        return false;
      }
      if (op == OP_LDWD)
      {
        if (addr + 4 > ctx->mem_size)
        {
          fprintf(stderr, "LDWd addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = read_u32_le(ctx->mem + addr);
      }
      else if (op == OP_LDBD)
      {
        if (addr >= ctx->mem_size)
        {
          fprintf(stderr, "LDBd addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = (uint32_t)(int32_t)(int8_t)ctx->mem[addr];
      }
      else if (op == OP_LDBU)
      {
        if (addr >= ctx->mem_size)
        {
          fprintf(stderr, "LDBU addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = ctx->mem[addr];
      }
      else if (op == OP_LDHU)
      {
        if (addr + 2 > ctx->mem_size)
        {
          fprintf(stderr, "LDHU addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = read_u16_le(ctx->mem + addr);
      }
      else if (op == OP_STWD)
      {
        if (addr + 4 > ctx->mem_size)
        {
          fprintf(stderr, "STWd addr OOB: 0x%X\n", addr);
          return false;
        }
        write_watch_if_code(ctx, addr, 4, "STWD");
        write_u32_le(ctx->mem + addr, ctx->regs[rd]);
      }
      else if (op == OP_STHD)
      {
        if (addr + 2 > ctx->mem_size)
        {
          fprintf(stderr, "STHd addr OOB: 0x%X\n", addr);
          return false;
        }
        write_watch_if_code(ctx, addr, 2, "STHD");
        write_u16_le(ctx->mem + addr, (uint16_t)(ctx->regs[rd] & 0xFFFFu));
      }
      else
      {
        if (addr >= ctx->mem_size)
        {
          fprintf(stderr, "STBd addr OOB: 0x%X\n", addr);
          return false;
        }
        write_watch_if_code(ctx, addr, 1, "STBD");
        ctx->mem[addr] = (uint8_t)(ctx->regs[rd] & 0xFFu);
      }
    }
    ctx->pc += 8;
    break;
  case OP_SYSCPY:
    if (ctx->regs[rd] + ctx->regs[rt] > ctx->mem_size || ctx->regs[rs] + ctx->regs[rt] > ctx->mem_size)
      return false;
    write_watch_if_code(ctx, ctx->regs[rd], ctx->regs[rt], "SYSCPY");
    memmove(ctx->mem + ctx->regs[rd], ctx->mem + ctx->regs[rs], ctx->regs[rt]);
    ctx->pc += 4;
    break;
  case OP_SYSSET:
    if (ctx->regs[rd] + ctx->regs[rt] > ctx->mem_size)
      return false;
    write_watch_if_code(ctx, ctx->regs[rd], ctx->regs[rt], "SYSSET");
    memset(ctx->mem + ctx->regs[rd], (int)(ctx->regs[rs] & 0xFFu), ctx->regs[rt]);
    ctx->pc += 4;
    break;
  case OP_STORE:
  {
    uint32_t first = reg_index(b1);
    uint32_t count = reg_index(b2);
    uint32_t r;
    for (r = first; r < first + count; ++r)
    {
      ctx->regs[VM_REG_SP] -= 4;
      if (ctx->regs[VM_REG_SP] + 4 > ctx->mem_size)
        return false;
      write_u32_le(ctx->mem + ctx->regs[VM_REG_SP], ctx->regs[r]);
    }
    ctx->pc += 4;
    break;
  }
  case OP_RESTORE:
  case OP_RET:
  {
    uint32_t first = reg_index(b1);
    uint32_t count = reg_index(b2);
    uint32_t r;
    for (r = 0; r < count; ++r)
    {
      uint32_t regno = first - r;
      if (ctx->regs[VM_REG_SP] + 4 > ctx->mem_size)
        return false;
      ctx->regs[regno] = read_u32_le(ctx->mem + ctx->regs[VM_REG_SP]);
      ctx->regs[VM_REG_SP] += 4;
    }
    if (op == OP_RET)
      ctx->pc = ctx->regs[VM_REG_RA];
    else
      ctx->pc += 4;
    break;
  }
  case OP_JPR:
    ctx->pc = ctx->regs[rd];
    break;
  case OP_CALLR:
    ctx->regs[VM_REG_RA] = ctx->pc + 4;
    ctx->pc = ctx->regs[rd];
    break;
  case OP_JPL:
    if (!fetch_code_u32(ctx, ctx->pc + 4, &ext))
      return false;
    ctx->pc = ctx->pc + (uint32_t)sext24(ext);
    break;
  case OP_CALLL:
  {
    uint32_t call_site = ctx->pc;
    uint32_t raw;
    uint32_t index;
    if (!fetch_code_u32(ctx, ctx->pc + 4, &raw))
      return false;
    if ((raw >> 24) != 0x00)
    {
      ctx->regs[VM_REG_RA] = ctx->pc + 8;
      ctx->pc = (uint32_t)sext24(raw);
      break;
    }
    index = imm24_u(raw);
    const VMGPPoolEntry *entry = vmgp_get_pool_entry(ctx, index);
    if (!entry)
    {
      fprintf(stderr, "CALLl pool index OOB at pc=0x%X\n", ctx->pc);
      return false;
    }
    log_vm_call(ctx, call_site, index, entry);
    if (entry->type == 0x02)
    {
      handle_import_call(ctx, index);
      ctx->pc += 8;
    }
    else if (entry->type == 0x11)
    {
      ctx->regs[VM_REG_RA] = ctx->pc + 8;
      ctx->pc = vmgp_resolve_pool_value(ctx, entry);
    }
    else
    {
      ctx->pc += 8;
    }
    break;
  }
  case OP_BEQI:
  case OP_BNEI:
  case OP_BGEI:
  case OP_BGTI:
  case OP_BGTUI:
  case OP_BLEI:
  case OP_BLEUI:
  case OP_BLTI:
  {
    int8_t immq = (int8_t)b2;
    uint32_t immu = b2;
    int8_t rel = (int8_t)b3;
    bool take = false;
    if (op == OP_BEQI)
      take = (reg_s32(ctx->regs[rd]) == immq);
    if (op == OP_BNEI)
      take = (reg_s32(ctx->regs[rd]) != immq);
    if (op == OP_BGEI)
      take = (reg_s32(ctx->regs[rd]) >= immq);
    if (op == OP_BGTI)
      take = (reg_s32(ctx->regs[rd]) > immq);
    if (op == OP_BGTUI)
      take = (ctx->regs[rd] > immu);
    if (op == OP_BLEI)
      take = (reg_s32(ctx->regs[rd]) <= immq);
    if (op == OP_BLEUI)
      take = (ctx->regs[rd] <= immu);
    if (op == OP_BLTI)
      take = (reg_s32(ctx->regs[rd]) < immq);
    ctx->pc += take ? (uint32_t)(rel * 4) : 4u;
    break;
  }
  case OP_BEQIB:
  case OP_BNEIB:
  case OP_BGEIB:
  case OP_BGEUIB:
  case OP_BGTIB:
  case OP_BGTUIB:
  case OP_BLEIB:
  case OP_BLEUIB:
  case OP_BLTIB:
  case OP_BLTUIB:
  {
    uint32_t lhs = ctx->regs[rd] & 0xFFu;
    uint32_t immu = b2;
    int8_t rel = (int8_t)b3;
    bool take = false;
    switch (op)
    {
    case OP_BEQIB:
      take = (lhs == immu);
      break;
    case OP_BNEIB:
      take = (lhs != immu);
      break;
    case OP_BGEIB:
      take = ((int8_t)lhs >= (int8_t)b2);
      break;
    case OP_BGEUIB:
      take = (lhs >= immu);
      break;
    case OP_BGTIB:
      take = ((int8_t)lhs > (int8_t)b2);
      break;
    case OP_BGTUIB:
      take = (lhs > immu);
      break;
    case OP_BLEIB:
      take = ((int8_t)lhs <= (int8_t)b2);
      break;
    case OP_BLEUIB:
      take = (lhs <= immu);
      break;
    case OP_BLTIB:
      take = ((int8_t)lhs < (int8_t)b2);
      break;
    case OP_BLTUIB:
      take = (lhs < immu);
      break;
    default:
      break;
    }
    ctx->pc += take ? (uint32_t)(rel * 4) : 4u;
    break;
  }
  case OP_BEQ:
  case OP_BNE:
  case OP_BGE:
  case OP_BGTU:
  case OP_BLE:
  case OP_BLEU:
  case OP_BLT:
  case OP_BLTU:
  {
    bool take = false;
    if (!fetch_code_u32(ctx, ctx->pc + 4, &ext))
      return false;
    if (op == OP_BEQ)
      take = (ctx->regs[rd] == ctx->regs[rs]);
    if (op == OP_BNE)
      take = (ctx->regs[rd] != ctx->regs[rs]);
    if (op == OP_BGE)
      take = (reg_s32(ctx->regs[rd]) >= reg_s32(ctx->regs[rs]));
    if (op == OP_BGTU)
      take = (ctx->regs[rd] > ctx->regs[rs]);
    if (op == OP_BLE)
      take = (reg_s32(ctx->regs[rd]) <= reg_s32(ctx->regs[rs]));
    if (op == OP_BLEU)
      take = (ctx->regs[rd] <= ctx->regs[rs]);
    if (op == OP_BLT)
      take = (reg_s32(ctx->regs[rd]) < reg_s32(ctx->regs[rs]));
    if (op == OP_BLTU)
      take = (ctx->regs[rd] < ctx->regs[rs]);
    ctx->pc = take ? (ctx->pc + 4 + (uint32_t)sext24(ext)) : (ctx->pc + 8);
    break;
  }
  case OP_SYSCALL0:
  case OP_SYSCALL1:
  case OP_SYSCALL2:
  case OP_SYSCALL3:
  case OP_SYSCALL4:
    log_syscall(ctx, op);
    ctx->pc += 4;
    break;
  default:
    fprintf(stderr, "unhandled opcode 0x%02X (%s) at pc=0x%08X raw=%08X b1=%u b2=%u b3=%u\n",
            op, opcode_name(op), ctx->pc, w, b1, b2, b3);
    return false;
  }

  ctx->regs[VM_REG_ZERO] = 0;
  ctx->steps++;
  return true;
}

bool vmgp_run_trace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls)
{
  if (!ctx || !ctx->mem)
  {
    return false;
  }

  printf("=== execution trace (first %u VM/system calls) ===\n", max_logged_calls);
  while (ctx->steps < max_steps && ctx->logged_calls < max_logged_calls && !ctx->halted)
  {
    if (!vmgp_step(ctx))
    {
      break;
    }
  }
  printf("=== stop ===\n");
  printf("steps=%u pc=0x%08X logged_calls=%u heap_cur=0x%08X r0=0x%08X\n",
         ctx->steps, ctx->pc, ctx->logged_calls, ctx->heap_cur, ctx->regs[VM_REG_R0]);
  return true;
}
