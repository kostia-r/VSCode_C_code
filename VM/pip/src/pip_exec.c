#include "mophun_vm_internal.h"

#include <stdio.h>
#include <string.h>

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

static bool fetch_code_u32(const VMGPContext *ctx, uint32_t pc, uint32_t *out)
{
  if (!ctx || !out || pc + 4 > ctx->header.code_size)
  {
    return false;
  }
  *out = vm_read_u32_le(ctx->data + ctx->code_file_offset + pc);
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

static uint32_t stack_arg0(const VMGPContext *ctx)
{
  return (ctx->regs[VM_REG_SP] + 4 <= ctx->mem_size) ? vm_read_u32_le(ctx->mem + ctx->regs[VM_REG_SP]) : 0u;
}

static void log_vm_call(VMGPContext *ctx, uint32_t call_site, uint32_t index, const VMGPPoolEntry *e)
{
  if (e->type == 0x02)
  {
    mophun_vm_logf(ctx,
                   "[vm-call %02u] pc=0x%08X CALLl pool[%u] import=%s sp=%08X stk0=%08X p0=%08X p1=%08X p2=%08X p3=%08X r0=%08X\n",
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
    mophun_vm_logf(ctx,
                   "[vm-call %02u] pc=0x%08X CALLl pool[%u] type=0x%02X(%s) value=0x%08X aux=0x%06X\n",
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
  mophun_vm_logf(ctx,
                 "[syscall %02u] pc=0x%08X %s argc=%u p0=%08X p1=%08X p2=%08X p3=%08X\n",
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

bool vmgp_step(VMGPContext *ctx)
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
    mophun_vm_logf(ctx, "pc out of code: 0x%08X\n", ctx->pc);
    return false;
  }

  op = (uint8_t)(w & 0xFFu);
  b1 = (uint8_t)((w >> 8) & 0xFFu);
  b2 = (uint8_t)((w >> 16) & 0xFFu);
  b3 = (uint8_t)((w >> 24) & 0xFFu);
  rd = vm_reg_index(b1);
  rs = vm_reg_index(b2);
  rt = vm_reg_index(b3);

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
      mophun_vm_logf(ctx, "DIVU by zero at pc=0x%X\n", ctx->pc);
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
    ctx->regs[rd] = (uint32_t)(-vm_reg_s32(ctx->regs[rs]));
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
    ctx->regs[rd] = (uint32_t)(vm_reg_s32(ctx->regs[rs]) >> b3);
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
      ctx->regs[rd] = ctx->regs[rs] + (uint32_t)vm_sext24(ext);
    else if (op == OP_ANDI)
      ctx->regs[rd] = ctx->regs[rs] & (uint32_t)vm_sext24(ext);
    else if (op == OP_MULI)
      ctx->regs[rd] = ctx->regs[rs] * (uint32_t)vm_sext24(ext);
    else if (op == OP_ORI)
      ctx->regs[rd] = ctx->regs[rs] | (uint32_t)vm_sext24(ext);
    else if (op == OP_DIVUI)
    {
      uint32_t immu = (uint32_t)vm_sext24(ext);
      ctx->regs[rd] = (immu == 0) ? 0u : (ctx->regs[rs] / immu);
    }
    else
    {
      int32_t imm = vm_sext24(ext);
      if (imm == 0)
      {
        mophun_vm_logf(ctx, "DIVi by zero at pc=0x%X\n", ctx->pc);
        return false;
      }
      ctx->regs[rd] = (uint32_t)(vm_reg_s32(ctx->regs[rs]) / imm);
    }
    ctx->pc += 8;
    break;
  case OP_LDI:
    if (!fetch_code_u32(ctx, ctx->pc + 4, &ext))
      return false;
    if ((ext >> 24) == 0x00)
    {
      const VMGPPoolEntry *entry = vmgp_get_pool_entry(ctx, vm_imm24_u(ext));
      if (!entry)
      {
        mophun_vm_logf(ctx, "LDI pool index OOB at pc=0x%X\n", ctx->pc);
        return false;
      }
      ctx->regs[rd] = vmgp_resolve_pool_value(ctx, entry);
    }
    else
    {
      ctx->regs[rd] = (uint32_t)vm_sext24(ext);
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
                         ? ((entry = vmgp_get_pool_entry(ctx, vm_imm24_u(ext))) ? vmgp_resolve_pool_value(ctx, entry) : 0u)
                         : (uint32_t)vm_sext24(ext);
      uint32_t addr = ctx->regs[rs] + off;
      if ((ext >> 24) == 0x00 && !entry)
      {
        mophun_vm_logf(ctx, "%s pool index OOB at pc=0x%X\n", opcode_name(op), ctx->pc);
        return false;
      }
      if (op == OP_LDWD)
      {
        if (addr + 4 > ctx->mem_size)
        {
          mophun_vm_logf(ctx, "LDWd addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = vm_read_u32_le(ctx->mem + addr);
      }
      else if (op == OP_LDBD)
      {
        if (addr >= ctx->mem_size)
        {
          mophun_vm_logf(ctx, "LDBd addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = (uint32_t)(int32_t)(int8_t)ctx->mem[addr];
      }
      else if (op == OP_LDBU)
      {
        if (addr >= ctx->mem_size)
        {
          mophun_vm_logf(ctx, "LDBU addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = ctx->mem[addr];
      }
      else if (op == OP_LDHU)
      {
        if (addr + 2 > ctx->mem_size)
        {
          mophun_vm_logf(ctx, "LDHU addr OOB: 0x%X\n", addr);
          return false;
        }
        ctx->regs[rd] = vm_read_u16_le(ctx->mem + addr);
      }
      else if (op == OP_STWD)
      {
        if (addr + 4 > ctx->mem_size)
        {
          mophun_vm_logf(ctx, "STWd addr OOB: 0x%X\n", addr);
          return false;
        }
        mophun_vm_memory_write_watch(ctx, addr, 4, "STWD");
        vm_write_u32_le(ctx->mem + addr, ctx->regs[rd]);
      }
      else if (op == OP_STHD)
      {
        if (addr + 2 > ctx->mem_size)
        {
          mophun_vm_logf(ctx, "STHd addr OOB: 0x%X\n", addr);
          return false;
        }
        mophun_vm_memory_write_watch(ctx, addr, 2, "STHD");
        vm_write_u16_le(ctx->mem + addr, (uint16_t)(ctx->regs[rd] & 0xFFFFu));
      }
      else
      {
        if (addr >= ctx->mem_size)
        {
          mophun_vm_logf(ctx, "STBd addr OOB: 0x%X\n", addr);
          return false;
        }
        mophun_vm_memory_write_watch(ctx, addr, 1, "STBD");
        ctx->mem[addr] = (uint8_t)(ctx->regs[rd] & 0xFFu);
      }
    }
    ctx->pc += 8;
    break;
  case OP_SYSCPY:
    if (ctx->regs[rd] + ctx->regs[rt] > ctx->mem_size || ctx->regs[rs] + ctx->regs[rt] > ctx->mem_size)
      return false;
    mophun_vm_memory_write_watch(ctx, ctx->regs[rd], ctx->regs[rt], "SYSCPY");
    memmove(ctx->mem + ctx->regs[rd], ctx->mem + ctx->regs[rs], ctx->regs[rt]);
    ctx->pc += 4;
    break;
  case OP_SYSSET:
    if (ctx->regs[rd] + ctx->regs[rt] > ctx->mem_size)
      return false;
    mophun_vm_memory_write_watch(ctx, ctx->regs[rd], ctx->regs[rt], "SYSSET");
    memset(ctx->mem + ctx->regs[rd], (int)(ctx->regs[rs] & 0xFFu), ctx->regs[rt]);
    ctx->pc += 4;
    break;
  case OP_STORE:
  {
    uint32_t first = vm_reg_index(b1);
    uint32_t count = vm_reg_index(b2);
    uint32_t r;
    for (r = first; r < first + count; ++r)
    {
      ctx->regs[VM_REG_SP] -= 4;
      if (ctx->regs[VM_REG_SP] + 4 > ctx->mem_size)
        return false;
      vm_write_u32_le(ctx->mem + ctx->regs[VM_REG_SP], ctx->regs[r]);
    }
    ctx->pc += 4;
    break;
  }
  case OP_RESTORE:
  case OP_RET:
  {
    uint32_t first = vm_reg_index(b1);
    uint32_t count = vm_reg_index(b2);
    uint32_t r;
    for (r = 0; r < count; ++r)
    {
      uint32_t regno = first - r;
      if (ctx->regs[VM_REG_SP] + 4 > ctx->mem_size)
        return false;
      ctx->regs[regno] = vm_read_u32_le(ctx->mem + ctx->regs[VM_REG_SP]);
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
    ctx->pc = ctx->pc + (uint32_t)vm_sext24(ext);
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
      ctx->pc = (uint32_t)vm_sext24(raw);
      break;
    }
    index = vm_imm24_u(raw);
    const VMGPPoolEntry *entry = vmgp_get_pool_entry(ctx, index);
    if (!entry)
    {
      mophun_vm_logf(ctx, "CALLl pool index OOB at pc=0x%X\n", ctx->pc);
      return false;
    }
    log_vm_call(ctx, call_site, index, entry);
    if (entry->type == 0x02)
    {
      vmgp_handle_import_call(ctx, index);
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
      take = (vm_reg_s32(ctx->regs[rd]) == immq);
    if (op == OP_BNEI)
      take = (vm_reg_s32(ctx->regs[rd]) != immq);
    if (op == OP_BGEI)
      take = (vm_reg_s32(ctx->regs[rd]) >= immq);
    if (op == OP_BGTI)
      take = (vm_reg_s32(ctx->regs[rd]) > immq);
    if (op == OP_BGTUI)
      take = (ctx->regs[rd] > immu);
    if (op == OP_BLEI)
      take = (vm_reg_s32(ctx->regs[rd]) <= immq);
    if (op == OP_BLEUI)
      take = (ctx->regs[rd] <= immu);
    if (op == OP_BLTI)
      take = (vm_reg_s32(ctx->regs[rd]) < immq);
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
      take = (vm_reg_s32(ctx->regs[rd]) >= vm_reg_s32(ctx->regs[rs]));
    if (op == OP_BGTU)
      take = (ctx->regs[rd] > ctx->regs[rs]);
    if (op == OP_BLE)
      take = (vm_reg_s32(ctx->regs[rd]) <= vm_reg_s32(ctx->regs[rs]));
    if (op == OP_BLEU)
      take = (ctx->regs[rd] <= ctx->regs[rs]);
    if (op == OP_BLT)
      take = (vm_reg_s32(ctx->regs[rd]) < vm_reg_s32(ctx->regs[rs]));
    if (op == OP_BLTU)
      take = (ctx->regs[rd] < ctx->regs[rs]);
    ctx->pc = take ? (ctx->pc + 4 + (uint32_t)vm_sext24(ext)) : (ctx->pc + 8);
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
    mophun_vm_logf(ctx,
                   "unhandled opcode 0x%02X (%s) at pc=0x%08X raw=%08X b1=%u b2=%u b3=%u\n",
                   op,
                   opcode_name(op),
                   ctx->pc,
                   w,
                   b1,
                   b2,
                   b3);
    return false;
  }

  ctx->regs[VM_REG_ZERO] = 0;
  ctx->steps++;
  return true;
}

