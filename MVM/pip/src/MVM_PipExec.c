/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_PipExec.c
 *           Module:  MVM_Pip
 *           Target:  Portable C
 *      Description:  PIP bytecode interpreter that executes one Mophun VM instruction stream.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include <stdio.h>
#include <string.h>

/**********************************************************************************************************************
 *  LOCAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Lists supported PIP instruction opcodes.
 */
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

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Fetches one 32-bit instruction word from VM code memory.
 */
static bool fetch_code_word(const VMGPContext *ctx, uint32_t pc, uint32_t *out);

/**
 * @brief Returns a printable PIP opcode name.
 */
static const char *opcode_name(uint8_t op);

/**
 * @brief Reads the first stack argument for trace output.
 */
static uint32_t stack_arg0(const VMGPContext *ctx);

/**
 * @brief Logs a VM CALLl instruction target.
 */
static void log_vm_call(VMGPContext *ctx, uint32_t call_site, uint32_t index, const VMGPPoolEntry *e);

/**
 * @brief Logs a direct VM syscall instruction.
 */
static void log_syscall(VMGPContext *ctx, uint8_t op);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_PipStep
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Executes one VM instruction.
 *********************************************************************************************************************/
bool MVM_PipStep(VMGPContext *ctx)
{
  uint32_t w, ext;
  uint8_t op, b1, b2, b3;
  uint32_t rd, rs, rt;
  uint32_t immu = 0;
  int32_t imm = 0;
  const VMGPPoolEntry *entry = NULL;
  uint32_t off = 0;
  uint32_t addr = 0;
  uint32_t first = 0;
  uint32_t count = 0;
  uint32_t r = 0;
  uint32_t regno = 0;
  uint32_t call_site = 0;
  uint32_t raw = 0;
  uint32_t index = 0;
  int8_t immq = 0;
  int8_t rel = 0;
  bool take = false;
  uint32_t lhs = 0;

  if (ctx->halted)
  {
    return true;
  }

  if (!fetch_code_word(ctx, ctx->pc, &w))
  {
    MVM_Logf(ctx, "pc out of code: 0x%08X\n", ctx->pc);
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
    {
      ctx->pc += 4;
      break;
    } /* End of case OP_SLEEP */

    case OP_ADD:
    {
      ctx->regs[rd] = ctx->regs[rs] + ctx->regs[rt];
      ctx->pc += 4;
      break;
    } /* End of case OP_ADD */

    case OP_AND:
    {
      ctx->regs[rd] = ctx->regs[rs] & ctx->regs[rt];
      ctx->pc += 4;
      break;
    } /* End of case OP_AND */

    case OP_MUL:
    {
      ctx->regs[rd] = ctx->regs[rs] * ctx->regs[rt];
      ctx->pc += 4;
      break;
    } /* End of case OP_MUL */

    case OP_DIVU:
    {
      if (ctx->regs[rt] == 0)
      {
        MVM_Logf(ctx, "DIVU by zero at pc=0x%X\n", ctx->pc);

        return false;
      }

      ctx->regs[rd] = ctx->regs[rs] / ctx->regs[rt];
      ctx->pc += 4;
      break;
    } /* End of case OP_DIVU */

    case OP_OR:
    {
      ctx->regs[rd] = ctx->regs[rs] | ctx->regs[rt];
      ctx->pc += 4;
      break;
    } /* End of case OP_OR */

    case OP_XOR:
    {
      ctx->regs[rd] = ctx->regs[rs] ^ ctx->regs[rt];
      ctx->pc += 4;
      break;
    } /* End of case OP_XOR */

    case OP_SUB:
    {
      ctx->regs[rd] = ctx->regs[rs] - ctx->regs[rt];
      ctx->pc += 4;
      break;
    } /* End of case OP_SUB */

    case OP_NOT:
    {
      ctx->regs[rd] = ~ctx->regs[rs];
      ctx->pc += 4;
      break;
    } /* End of case OP_NOT */

    case OP_NEG:
    {
      ctx->regs[rd] = (uint32_t)(-vm_reg_s32(ctx->regs[rs]));
      ctx->pc += 4;
      break;
    } /* End of case OP_NEG */

    case OP_EXSB:
    {
      ctx->regs[rd] = (uint32_t)(int32_t)(int8_t)(ctx->regs[rs] & 0xFFu);
      ctx->pc += 4;
      break;
    } /* End of case OP_EXSB */

    case OP_EXSH:
    {
      ctx->regs[rd] = (uint32_t)(int32_t)(int16_t)(ctx->regs[rs] & 0xFFFFu);
      ctx->pc += 4;
      break;
    } /* End of case OP_EXSH */

    case OP_MOV:
    {
      ctx->regs[rd] = ctx->regs[rs];
      ctx->pc += 4;
      break;
    } /* End of case OP_MOV */

    case OP_ADDB:
    {
      ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) + (ctx->regs[rt] & 0xFFu)) & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ADDB */

    case OP_SUBB:
    {
      ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) - (ctx->regs[rt] & 0xFFu)) & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_SUBB */

    case OP_ANDB:
    {
      ctx->regs[rd] = (ctx->regs[rs] & ctx->regs[rt]) & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ANDB */

    case OP_ORB:
    {
      ctx->regs[rd] = (ctx->regs[rs] | ctx->regs[rt]) & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ORB */

    case OP_MOVB:
    {
      ctx->regs[rd] = ctx->regs[rs] & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_MOVB */

    case OP_ADDH:
    {
      ctx->regs[rd] = ((ctx->regs[rs] & 0xFFFFu) + (ctx->regs[rt] & 0xFFFFu)) & 0xFFFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ADDH */

    case OP_SUBH:
    {
      ctx->regs[rd] = ((ctx->regs[rs] & 0xFFFFu) - (ctx->regs[rt] & 0xFFFFu)) & 0xFFFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_SUBH */

    case OP_ANDH:
    {
      ctx->regs[rd] = (ctx->regs[rs] & ctx->regs[rt]) & 0xFFFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ANDH */

    case OP_ORH:
    {
      ctx->regs[rd] = (ctx->regs[rs] | ctx->regs[rt]) & 0xFFFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ORH */

    case OP_MOVH:
    {
      ctx->regs[rd] = ctx->regs[rs] & 0xFFFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_MOVH */

    case OP_SLLI:
    {
      ctx->regs[rd] = ctx->regs[rs] << b3;
      ctx->pc += 4;
      break;
    } /* End of case OP_SLLI */

    case OP_SRAI:
    {
      ctx->regs[rd] = (uint32_t)(vm_reg_s32(ctx->regs[rs]) >> b3);
      ctx->pc += 4;
      break;
    } /* End of case OP_SRAI */

    case OP_SRLI:
    {
      ctx->regs[rd] = ctx->regs[rs] >> b3;
      ctx->pc += 4;
      break;
    } /* End of case OP_SRLI */

    case OP_ADDQ:
    {
      ctx->regs[rd] = ctx->regs[rs] + (int8_t)b3;
      ctx->pc += 4;
      break;
    } /* End of case OP_ADDQ */

    case OP_MULQ:
    {
      ctx->regs[rd] = ctx->regs[rs] * (uint32_t)((uint8_t)b3);
      ctx->pc += 4;
      break;
    } /* End of case OP_MULQ */

    case OP_ADDBI:
    {
      ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) + b3) & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ADDBI */

    case OP_ANDBI:
    {
      ctx->regs[rd] = ctx->regs[rs] & (uint32_t)b3;
      ctx->pc += 4;
      break;
    } /* End of case OP_ANDBI */

    case OP_ORBI:
    {
      ctx->regs[rd] = (ctx->regs[rs] | (uint32_t)b3) & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ORBI */

    case OP_SLLB:
    {
      ctx->regs[rd] = ((ctx->regs[rs] & 0xFFu) << b3) & 0xFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_SLLB */

    case OP_SRLB:
    {
      ctx->regs[rd] = (ctx->regs[rs] & 0xFFu) >> b3;
      ctx->pc += 4;
      break;
    } /* End of case OP_SRLB */

    case OP_SRAB:
    {
      ctx->regs[rd] = (uint32_t)((uint8_t)((int8_t)(ctx->regs[rs] & 0xFFu) >> b3));
      ctx->pc += 4;
      break;
    } /* End of case OP_SRAB */

    case OP_ADDHI:
    {
      ctx->regs[rd] = (ctx->regs[rs] + (uint32_t)(int8_t)b3) & 0xFFFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_ADDHI */

    case OP_ANDHI:
    {
      ctx->regs[rd] = ctx->regs[rs] & (uint32_t)b3;
      ctx->pc += 4;
      break;
    } /* End of case OP_ANDHI */

    case OP_SLLH:
    {
      ctx->regs[rd] = (ctx->regs[rs] << b3) & 0xFFFFu;
      ctx->pc += 4;
      break;
    } /* End of case OP_SLLH */

    case OP_SRLH:
    {
      ctx->regs[rd] = (ctx->regs[rs] & 0xFFFFu) >> b3;
      ctx->pc += 4;
      break;
    } /* End of case OP_SRLH */

    case OP_SRAH:
    {
      ctx->regs[rd] = (uint32_t)((uint16_t)((int16_t)(ctx->regs[rs] & 0xFFFFu) >> b3));
      ctx->pc += 4;
      break;
    } /* End of case OP_SRAH */

    case OP_LDQ:
    {
      ctx->regs[rd] = (uint32_t)(int32_t)(int16_t)((w >> 16) & 0xFFFFu);
      ctx->pc += 4;
      break;
    } /* End of case OP_LDQ */

    case OP_ADDI:

    case OP_ANDI:

    case OP_MULI:

    case OP_DIVI:

    case OP_DIVUI:

    case OP_ORI:
    {
      if (!fetch_code_word(ctx, ctx->pc + 4, &ext))
      {
        return false;
      }

      if (op == OP_ADDI)
      {
        ctx->regs[rd] = ctx->regs[rs] + (uint32_t)vm_sext24(ext);
      }
      else if (op == OP_ANDI)
      {
        ctx->regs[rd] = ctx->regs[rs] & (uint32_t)vm_sext24(ext);
      }
      else if (op == OP_MULI)
      {
        ctx->regs[rd] = ctx->regs[rs] * (uint32_t)vm_sext24(ext);
      }
      else if (op == OP_ORI)
      {
        ctx->regs[rd] = ctx->regs[rs] | (uint32_t)vm_sext24(ext);
      }
      else if (op == OP_DIVUI)
      {
        immu = (uint32_t)vm_sext24(ext);
        ctx->regs[rd] = (immu == 0) ? 0u : (ctx->regs[rs] / immu);
      }
      else
      {
        imm = vm_sext24(ext);

        if (imm == 0)
        {
          MVM_Logf(ctx, "DIVi by zero at pc=0x%X\n", ctx->pc);

          return false;
        }

        ctx->regs[rd] = (uint32_t)(vm_reg_s32(ctx->regs[rs]) / imm);
      }
      ctx->pc += 8;
      break;
    } /* End of case OP_ORI */

    case OP_LDI:
    {
      if (!fetch_code_word(ctx, ctx->pc + 4, &ext))
      {
        return false;
      }

      if ((ext >> 24) == 0x00)
      {
        entry = MVM_GetVmgpPoolEntry(ctx, vm_imm24_u(ext));

        if (!entry)
        {
          MVM_Logf(ctx, "LDI pool index OOB at pc=0x%X\n", ctx->pc);

          return false;
        }

        ctx->regs[rd] = MVM_ResolveVmgpPoolValue(ctx, entry);
      }
      else
      {
        ctx->regs[rd] = (uint32_t)vm_sext24(ext);
      }

      ctx->pc += 8;
      break;
    } /* End of case OP_LDI */

    case OP_LDWD:

    case OP_LDBD:

    case OP_LDBU:

    case OP_LDHU:

    case OP_STWD:

    case OP_STHD:

    case OP_STBD:
    {
      if (!fetch_code_word(ctx, ctx->pc + 4, &ext))
      {
        return false;
      }
      entry = NULL;
      off = ((ext >> 24) == 0x00)
          ? ((entry = MVM_GetVmgpPoolEntry(ctx, vm_imm24_u(ext))) ? MVM_ResolveVmgpPoolValue(ctx, entry) : 0u)
          : (uint32_t)vm_sext24(ext);
      addr = ctx->regs[rs] + off;

      if ((ext >> 24) == 0x00 && !entry)
      {
        MVM_Logf(ctx, "%s pool index OOB at pc=0x%X\n", opcode_name(op), ctx->pc);

        return false;
      }

      if (op == OP_LDWD)
      {
        if (addr + 4 > ctx->mem_size)
        {
          MVM_Logf(ctx, "LDWd addr OOB: 0x%X\n", addr);

          return false;
        }

        ctx->regs[rd] = vm_read_u32_le(ctx->mem + addr);
      }
      else if (op == OP_LDBD)
      {
        if (addr >= ctx->mem_size)
        {
          MVM_Logf(ctx, "LDBd addr OOB: 0x%X\n", addr);

          return false;
        }

        ctx->regs[rd] = (uint32_t)(int32_t)(int8_t)ctx->mem[addr];
      }
      else if (op == OP_LDBU)
      {
        if (addr >= ctx->mem_size)
        {
          MVM_Logf(ctx, "LDBU addr OOB: 0x%X\n", addr);

          return false;
        }

        ctx->regs[rd] = ctx->mem[addr];
      }
      else if (op == OP_LDHU)
      {
        if (addr + 2 > ctx->mem_size)
        {
          MVM_Logf(ctx, "LDHU addr OOB: 0x%X\n", addr);

          return false;
        }

        ctx->regs[rd] = vm_read_u16_le(ctx->mem + addr);
      }
      else if (op == OP_STWD)
      {
        if (addr + 4 > ctx->mem_size)
        {
          MVM_Logf(ctx, "STWd addr OOB: 0x%X\n", addr);

          return false;
        }

        MVM_WatchMemoryWrite(ctx, addr, 4, "STWD");
        vm_write_u32_le(ctx->mem + addr, ctx->regs[rd]);
      }
      else if (op == OP_STHD)
      {
        if (addr + 2 > ctx->mem_size)
        {
          MVM_Logf(ctx, "STHd addr OOB: 0x%X\n", addr);

          return false;
        }

        MVM_WatchMemoryWrite(ctx, addr, 2, "STHD");
        vm_write_u16_le(ctx->mem + addr, (uint16_t)(ctx->regs[rd] & 0xFFFFu));
      }
      else
      {
        if (addr >= ctx->mem_size)
        {
          MVM_Logf(ctx, "STBd addr OOB: 0x%X\n", addr);

          return false;
        }

        MVM_WatchMemoryWrite(ctx, addr, 1, "STBD");
        ctx->mem[addr] = (uint8_t)(ctx->regs[rd] & 0xFFu);
      }

      ctx->pc += 8;
      break;
    } /* End of case OP_STBD */

    case OP_SYSCPY:
    {
      if (ctx->regs[rd] + ctx->regs[rt] > ctx->mem_size || ctx->regs[rs] + ctx->regs[rt] > ctx->mem_size)
      {
        return false;
      }

      MVM_WatchMemoryWrite(ctx, ctx->regs[rd], ctx->regs[rt], "SYSCPY");
      memmove(ctx->mem + ctx->regs[rd], ctx->mem + ctx->regs[rs], ctx->regs[rt]);
      ctx->pc += 4;
      break;
    } /* End of case OP_SYSCPY */

    case OP_SYSSET:
    {
      if (ctx->regs[rd] + ctx->regs[rt] > ctx->mem_size)
      {
        return false;
      }

      MVM_WatchMemoryWrite(ctx, ctx->regs[rd], ctx->regs[rt], "SYSSET");
      memset(ctx->mem + ctx->regs[rd], (int)(ctx->regs[rs] & 0xFFu), ctx->regs[rt]);
      ctx->pc += 4;
      break;
    } /* End of case OP_SYSSET */

    case OP_STORE:
    {
      first = vm_reg_index(b1);
      count = vm_reg_index(b2);

      for (r = first; r < first + count; ++r)
      {
        ctx->regs[VM_REG_SP] -= 4;

        if (ctx->regs[VM_REG_SP] + 4 > ctx->mem_size)
        {
          return false;
        }

        vm_write_u32_le(ctx->mem + ctx->regs[VM_REG_SP], ctx->regs[r]);
      } /* End of loop */

      ctx->pc += 4;
      break;
    } /* End of case OP_STORE */

    case OP_RESTORE:

    case OP_RET:
    {
      first = vm_reg_index(b1);
      count = vm_reg_index(b2);

      for (r = 0; r < count; ++r)
      {
        regno = first - r;

        if (ctx->regs[VM_REG_SP] + 4 > ctx->mem_size)
        {
          return false;
        }

        ctx->regs[regno] = vm_read_u32_le(ctx->mem + ctx->regs[VM_REG_SP]);
        ctx->regs[VM_REG_SP] += 4;
      } /* End of loop */

      if (op == OP_RET)
      {
        ctx->pc = ctx->regs[VM_REG_RA];
      }
      else
      {
        ctx->pc += 4;
      }

      break;
    } /* End of case OP_RET */

    case OP_JPR:
    {
      ctx->pc = ctx->regs[rd];
      break;
    } /* End of case OP_JPR */

    case OP_CALLR:
    {
      ctx->regs[VM_REG_RA] = ctx->pc + 4;
      ctx->pc = ctx->regs[rd];
      break;
    } /* End of case OP_CALLR */

    case OP_JPL:
    {
      if (!fetch_code_word(ctx, ctx->pc + 4, &ext))
      {
        return false;
      }
      ctx->pc = ctx->pc + (uint32_t)vm_sext24(ext);
      break;
    } /* End of case OP_JPL */

    case OP_CALLL:
    {
      call_site = ctx->pc;
      raw = 0;
      index = 0;

      if (!fetch_code_word(ctx, ctx->pc + 4, &raw))
      {
        return false;
      }

      if ((raw >> 24) != 0x00)
      {
        ctx->regs[VM_REG_RA] = ctx->pc + 8;
        ctx->pc = (uint32_t)vm_sext24(raw);
        break;
      }

      index = vm_imm24_u(raw);
      entry = MVM_GetVmgpPoolEntry(ctx, index);

      if (!entry)
      {
        MVM_Logf(ctx, "CALLl pool index OOB at pc=0x%X\n", ctx->pc);

        return false;
      }

      log_vm_call(ctx, call_site, index, entry);

      if (entry->type == 0x02)
      {
        MVM_HandleRuntimeImportCall(ctx, index);
        ctx->pc += 8;
      }
      else if (entry->type == 0x11)
      {
        ctx->regs[VM_REG_RA] = ctx->pc + 8;
        ctx->pc = MVM_ResolveVmgpPoolValue(ctx, entry);
      }
      else
      {
        ctx->pc += 8;
      }

      break;
    } /* End of case OP_CALLL */

    case OP_BEQI:

    case OP_BNEI:

    case OP_BGEI:

    case OP_BGTI:

    case OP_BGTUI:

    case OP_BLEI:

    case OP_BLEUI:

    case OP_BLTI:
    {
      immq = (int8_t)b2;
      immu = b2;
      rel = (int8_t)b3;
      take = false;

      if (op == OP_BEQI)
      {
        take = (vm_reg_s32(ctx->regs[rd]) == immq);
      }

      if (op == OP_BNEI)
      {
        take = (vm_reg_s32(ctx->regs[rd]) != immq);
      }

      if (op == OP_BGEI)
      {
        take = (vm_reg_s32(ctx->regs[rd]) >= immq);
      }

      if (op == OP_BGTI)
      {
        take = (vm_reg_s32(ctx->regs[rd]) > immq);
      }

      if (op == OP_BGTUI)
      {
        take = (ctx->regs[rd] > immu);
      }

      if (op == OP_BLEI)
      {
        take = (vm_reg_s32(ctx->regs[rd]) <= immq);
      }

      if (op == OP_BLEUI)
      {
        take = (ctx->regs[rd] <= immu);
      }

      if (op == OP_BLTI)
      {
        take = (vm_reg_s32(ctx->regs[rd]) < immq);
      }

      ctx->pc += take ? (uint32_t)(rel * 4) : 4u;
      break;
    } /* End of case OP_BLTI */

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
      lhs = ctx->regs[rd] & 0xFFu;
      immu = b2;
      rel = (int8_t)b3;
      take = false;

      switch (op)
      {
        case OP_BEQIB:
        {
          take = (lhs == immu);
          break;
        } /* End of case OP_BEQIB */

        case OP_BNEIB:
        {
          take = (lhs != immu);
          break;
        } /* End of case OP_BNEIB */

        case OP_BGEIB:
        {
          take = ((int8_t)lhs >= (int8_t)b2);
          break;
        } /* End of case OP_BGEIB */

        case OP_BGEUIB:
        {
          take = (lhs >= immu);
          break;
        } /* End of case OP_BGEUIB */

        case OP_BGTIB:
        {
          take = ((int8_t)lhs > (int8_t)b2);
          break;
        } /* End of case OP_BGTIB */

        case OP_BGTUIB:
        {
          take = (lhs > immu);
          break;
        } /* End of case OP_BGTUIB */

        case OP_BLEIB:
        {
          take = ((int8_t)lhs <= (int8_t)b2);
          break;
        } /* End of case OP_BLEIB */

        case OP_BLEUIB:
        {
          take = (lhs <= immu);
          break;
        } /* End of case OP_BLEUIB */

        case OP_BLTIB:
        {
          take = ((int8_t)lhs < (int8_t)b2);
          break;
        } /* End of case OP_BLTIB */

        case OP_BLTUIB:
        {
          take = (lhs < immu);
          break;
        } /* End of case OP_BLTUIB */

        default:
        {
          break;
        } /* End of default */
      } /* End of switch */

      ctx->pc += take ? (uint32_t)(rel * 4) : 4u;
      break;
    } /* End of case OP_BLTUIB */

    case OP_BEQ:

    case OP_BNE:

    case OP_BGE:

    case OP_BGTU:

    case OP_BLE:

    case OP_BLEU:

    case OP_BLT:

    case OP_BLTU:
    {
      take = false;

      if (!fetch_code_word(ctx, ctx->pc + 4, &ext))
      {
        return false;
      }

      if (op == OP_BEQ)
      {
        take = (ctx->regs[rd] == ctx->regs[rs]);
      }

      if (op == OP_BNE)
      {
        take = (ctx->regs[rd] != ctx->regs[rs]);
      }

      if (op == OP_BGE)
      {
        take = (vm_reg_s32(ctx->regs[rd]) >= vm_reg_s32(ctx->regs[rs]));
      }

      if (op == OP_BGTU)
      {
        take = (ctx->regs[rd] > ctx->regs[rs]);
      }

      if (op == OP_BLE)
      {
        take = (vm_reg_s32(ctx->regs[rd]) <= vm_reg_s32(ctx->regs[rs]));
      }

      if (op == OP_BLEU)
      {
        take = (ctx->regs[rd] <= ctx->regs[rs]);
      }

      if (op == OP_BLT)
      {
        take = (vm_reg_s32(ctx->regs[rd]) < vm_reg_s32(ctx->regs[rs]));
      }

      if (op == OP_BLTU)
      {
        take = (ctx->regs[rd] < ctx->regs[rs]);
      }

      ctx->pc = take ? (ctx->pc + 4 + (uint32_t)vm_sext24(ext)) : (ctx->pc + 8);
      break;
    } /* End of case OP_BLTU */

    case OP_SYSCALL0:

    case OP_SYSCALL1:

    case OP_SYSCALL2:

    case OP_SYSCALL3:

    case OP_SYSCALL4:
    {
      log_syscall(ctx, op);
      ctx->pc += 4;
      break;
    } /* End of case OP_SYSCALL4 */

    default:
    {
      MVM_Logf(ctx,
      "unhandled opcode 0x%02X (%s) at pc=0x%08X raw=%08X b1=%u b2=%u b3=%u\n",
      op,
      opcode_name(op),
      ctx->pc,
      w,
      b1,
      b2,
      b3);

      return false;
    } /* End of default */
  } /* End of switch */

  ctx->regs[VM_REG_ZERO] = 0;
  ctx->steps++;
  return true;
} /* End of MVM_PipStep */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: fetch_code_word
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static bool fetch_code_word(const VMGPContext *ctx, uint32_t pc, uint32_t *out)
{
  uint8_t bytes[4];

  if (!ctx || !out || pc + 4 > ctx->header.code_size)
  {
    return false;
  }

  if (!MVM_ReadImageRange(ctx, ctx->code_file_offset + pc, bytes, sizeof(bytes)))
  {
    return false;
  }

  *out = vm_read_u32_le(bytes);

  return true;
} /* End of fetch_code_word */

/**********************************************************************************************************************
 *  Name: opcode_name
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static const char *opcode_name(uint8_t op)
{
  const char *opcodeName = "UNKNOWN";

  switch (op)
  {
    case OP_NOP:
    {
      opcodeName = "NOP";
      break;
    } /* End of case OP_NOP */

    case OP_ADD:
    {
      opcodeName = "ADD";
      break;
    } /* End of case OP_ADD */

    case OP_AND:
    {
      opcodeName = "AND";
      break;
    } /* End of case OP_AND */

    case OP_MUL:
    {
      opcodeName = "MUL";
      break;
    } /* End of case OP_MUL */

    case OP_DIVU:
    {
      opcodeName = "DIVU";
      break;
    } /* End of case OP_DIVU */

    case OP_OR:
    {
      opcodeName = "OR";
      break;
    } /* End of case OP_OR */

    case OP_XOR:
    {
      opcodeName = "XOR";
      break;
    } /* End of case OP_XOR */

    case OP_SUB:
    {
      opcodeName = "SUB";
      break;
    } /* End of case OP_SUB */

    case OP_NOT:
    {
      opcodeName = "NOT";
      break;
    } /* End of case OP_NOT */

    case OP_NEG:
    {
      opcodeName = "NEG";
      break;
    } /* End of case OP_NEG */

    case OP_EXSB:
    {
      opcodeName = "EXSB";
      break;
    } /* End of case OP_EXSB */

    case OP_EXSH:
    {
      opcodeName = "EXSH";
      break;
    } /* End of case OP_EXSH */

    case OP_MOV:
    {
      opcodeName = "MOV";
      break;
    } /* End of case OP_MOV */

    case OP_ADDB:
    {
      opcodeName = "ADDB";
      break;
    } /* End of case OP_ADDB */

    case OP_SUBB:
    {
      opcodeName = "SUBB";
      break;
    } /* End of case OP_SUBB */

    case OP_ANDB:
    {
      opcodeName = "ANDB";
      break;
    } /* End of case OP_ANDB */

    case OP_ORB:
    {
      opcodeName = "ORB";
      break;
    } /* End of case OP_ORB */

    case OP_MOVB:
    {
      opcodeName = "MOVB";
      break;
    } /* End of case OP_MOVB */

    case OP_ADDH:
    {
      opcodeName = "ADDH";
      break;
    } /* End of case OP_ADDH */

    case OP_SUBH:
    {
      opcodeName = "SUBH";
      break;
    } /* End of case OP_SUBH */

    case OP_ANDH:
    {
      opcodeName = "ANDH";
      break;
    } /* End of case OP_ANDH */

    case OP_ORH:
    {
      opcodeName = "ORH";
      break;
    } /* End of case OP_ORH */

    case OP_MOVH:
    {
      opcodeName = "MOVH";
      break;
    } /* End of case OP_MOVH */

    case OP_SLLI:
    {
      opcodeName = "SLLi";
      break;
    } /* End of case OP_SLLI */

    case OP_SRAI:
    {
      opcodeName = "SRAi";
      break;
    } /* End of case OP_SRAI */

    case OP_SRLI:
    {
      opcodeName = "SRLi";
      break;
    } /* End of case OP_SRLI */

    case OP_ADDQ:
    {
      opcodeName = "ADDQ";
      break;
    } /* End of case OP_ADDQ */

    case OP_MULQ:
    {
      opcodeName = "MULQ";
      break;
    } /* End of case OP_MULQ */

    case OP_ADDBI:
    {
      opcodeName = "ADDBi";
      break;
    } /* End of case OP_ADDBI */

    case OP_ANDBI:
    {
      opcodeName = "ANDBi";
      break;
    } /* End of case OP_ANDBI */

    case OP_ORBI:
    {
      opcodeName = "ORBi";
      break;
    } /* End of case OP_ORBI */

    case OP_SLLB:
    {
      opcodeName = "SLLB";
      break;
    } /* End of case OP_SLLB */

    case OP_SRLB:
    {
      opcodeName = "SRLB";
      break;
    } /* End of case OP_SRLB */

    case OP_SRAB:
    {
      opcodeName = "SRAB";
      break;
    } /* End of case OP_SRAB */

    case OP_ADDHI:
    {
      opcodeName = "ADDHi";
      break;
    } /* End of case OP_ADDHI */

    case OP_ANDHI:
    {
      opcodeName = "ANDHi";
      break;
    } /* End of case OP_ANDHI */

    case OP_SLLH:
    {
      opcodeName = "SLLH";
      break;
    } /* End of case OP_SLLH */

    case OP_SRLH:
    {
      opcodeName = "SRLH";
      break;
    } /* End of case OP_SRLH */

    case OP_SRAH:
    {
      opcodeName = "SRAH";
      break;
    } /* End of case OP_SRAH */

    case OP_BEQI:
    {
      opcodeName = "BEQI";
      break;
    } /* End of case OP_BEQI */

    case OP_BNEI:
    {
      opcodeName = "BNEI";
      break;
    } /* End of case OP_BNEI */

    case OP_BGEI:
    {
      opcodeName = "BGEI";
      break;
    } /* End of case OP_BGEI */

    case OP_BGTI:
    {
      opcodeName = "BGTI";
      break;
    } /* End of case OP_BGTI */

    case OP_BGTUI:
    {
      opcodeName = "BGTUI";
      break;
    } /* End of case OP_BGTUI */

    case OP_BLEI:
    {
      opcodeName = "BLEI";
      break;
    } /* End of case OP_BLEI */

    case OP_BLEUI:
    {
      opcodeName = "BLEUI";
      break;
    } /* End of case OP_BLEUI */

    case OP_BLTI:
    {
      opcodeName = "BLTI";
      break;
    } /* End of case OP_BLTI */

    case OP_BEQIB:
    {
      opcodeName = "BEQIB";
      break;
    } /* End of case OP_BEQIB */

    case OP_BNEIB:
    {
      opcodeName = "BNEIB";
      break;
    } /* End of case OP_BNEIB */

    case OP_BGEIB:
    {
      opcodeName = "BGEIB";
      break;
    } /* End of case OP_BGEIB */

    case OP_BGEUIB:
    {
      opcodeName = "BGEUIB";
      break;
    } /* End of case OP_BGEUIB */

    case OP_BGTIB:
    {
      opcodeName = "BGTIB";
      break;
    } /* End of case OP_BGTIB */

    case OP_BGTUIB:
    {
      opcodeName = "BGTUIB";
      break;
    } /* End of case OP_BGTUIB */

    case OP_BLEIB:
    {
      opcodeName = "BLEIB";
      break;
    } /* End of case OP_BLEIB */

    case OP_BLEUIB:
    {
      opcodeName = "BLEUIB";
      break;
    } /* End of case OP_BLEUIB */

    case OP_BLTIB:
    {
      opcodeName = "BLTIB";
      break;
    } /* End of case OP_BLTIB */

    case OP_BLTUIB:
    {
      opcodeName = "BLTUIB";
      break;
    } /* End of case OP_BLTUIB */

    case OP_LDQ:
    {
      opcodeName = "LDQ";
      break;
    } /* End of case OP_LDQ */

    case OP_JPR:
    {
      opcodeName = "JPr";
      break;
    } /* End of case OP_JPR */

    case OP_CALLR:
    {
      opcodeName = "CALLr";
      break;
    } /* End of case OP_CALLR */

    case OP_STORE:
    {
      opcodeName = "STORE";
      break;
    } /* End of case OP_STORE */

    case OP_RESTORE:
    {
      opcodeName = "RESTORE";
      break;
    } /* End of case OP_RESTORE */

    case OP_RET:
    {
      opcodeName = "RET";
      break;
    } /* End of case OP_RET */

    case OP_SLEEP:
    {
      opcodeName = "SLEEP";
      break;
    } /* End of case OP_SLEEP */

    case OP_SYSCPY:
    {
      opcodeName = "SYSCPY";
      break;
    } /* End of case OP_SYSCPY */

    case OP_SYSSET:
    {
      opcodeName = "SYSSET";
      break;
    } /* End of case OP_SYSSET */

    case OP_ADDI:
    {
      opcodeName = "ADDi";
      break;
    } /* End of case OP_ADDI */

    case OP_ANDI:
    {
      opcodeName = "ANDi";
      break;
    } /* End of case OP_ANDI */

    case OP_MULI:
    {
      opcodeName = "MULi";
      break;
    } /* End of case OP_MULI */

    case OP_DIVI:
    {
      opcodeName = "DIVi";
      break;
    } /* End of case OP_DIVI */

    case OP_DIVUI:
    {
      opcodeName = "DIVUi";
      break;
    } /* End of case OP_DIVUI */

    case OP_ORI:
    {
      opcodeName = "ORi";
      break;
    } /* End of case OP_ORI */

    case OP_STBD:
    {
      opcodeName = "STBd";
      break;
    } /* End of case OP_STBD */

    case OP_STHD:
    {
      opcodeName = "STHd";
      break;
    } /* End of case OP_STHD */

    case OP_STWD:
    {
      opcodeName = "STWd";
      break;
    } /* End of case OP_STWD */

    case OP_LDBD:
    {
      opcodeName = "LDBd";
      break;
    } /* End of case OP_LDBD */

    case OP_LDWD:
    {
      opcodeName = "LDWd";
      break;
    } /* End of case OP_LDWD */

    case OP_LDBU:
    {
      opcodeName = "LDBU";
      break;
    } /* End of case OP_LDBU */

    case OP_LDHU:
    {
      opcodeName = "LDHU";
      break;
    } /* End of case OP_LDHU */

    case OP_LDI:
    {
      opcodeName = "LDI";
      break;
    } /* End of case OP_LDI */

    case OP_JPL:
    {
      opcodeName = "JPl";
      break;
    } /* End of case OP_JPL */

    case OP_CALLL:
    {
      opcodeName = "CALLl";
      break;
    } /* End of case OP_CALLL */

    case OP_BEQ:
    {
      opcodeName = "BEQ";
      break;
    } /* End of case OP_BEQ */

    case OP_BNE:
    {
      opcodeName = "BNE";
      break;
    } /* End of case OP_BNE */

    case OP_BGE:
    {
      opcodeName = "BGE";
      break;
    } /* End of case OP_BGE */

    case OP_BGTU:
    {
      opcodeName = "BGTU";
      break;
    } /* End of case OP_BGTU */

    case OP_BLE:
    {
      opcodeName = "BLE";
      break;
    } /* End of case OP_BLE */

    case OP_BLEU:
    {
      opcodeName = "BLEU";
      break;
    } /* End of case OP_BLEU */

    case OP_BLT:
    {
      opcodeName = "BLT";
      break;
    } /* End of case OP_BLT */

    case OP_BLTU:
    {
      opcodeName = "BLTU";
      break;
    } /* End of case OP_BLTU */

    case OP_SYSCALL4:
    {
      opcodeName = "SYSCALL4";
      break;
    } /* End of case OP_SYSCALL4 */

    case OP_SYSCALL0:
    {
      opcodeName = "SYSCALL0";
      break;
    } /* End of case OP_SYSCALL0 */

    case OP_SYSCALL1:
    {
      opcodeName = "SYSCALL1";
      break;
    } /* End of case OP_SYSCALL1 */

    case OP_SYSCALL2:
    {
      opcodeName = "SYSCALL2";
      break;
    } /* End of case OP_SYSCALL2 */

    case OP_SYSCALL3:
    {
      opcodeName = "SYSCALL3";
      break;
    } /* End of case OP_SYSCALL3 */

    default:
    {
      break;
    } /* End of default */
  } /* End of switch */

  return opcodeName;
} /* End of opcode_name */

/**********************************************************************************************************************
 *  Name: stack_arg0
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static uint32_t stack_arg0(const VMGPContext *ctx)
{
  uint32_t arg0 = 0;

  arg0 = (ctx->regs[VM_REG_SP] + 4 <= ctx->mem_size) ? vm_read_u32_le(ctx->mem + ctx->regs[VM_REG_SP]) : 0u;

  return arg0;
} /* End of stack_arg0 */

/**********************************************************************************************************************
 *  Name: log_vm_call
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static void log_vm_call(VMGPContext *ctx, uint32_t call_site, uint32_t index, const VMGPPoolEntry *e)
{
  if (e->type == 0x02)
  {
    MVM_Logf(ctx,
    "[vm-call %02u] pc=0x%08X CALLl pool[%u] import=%s sp=%08X stk0=%08X p0=%08X p1=%08X p2=%08X p3=%08X r0=%08X\n",
    ctx->logged_calls + 1,
    call_site,
    index,
    MVM_GetVmgpImportName(ctx, index),
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
    MVM_Logf(ctx,
    "[vm-call %02u] pc=0x%08X CALLl pool[%u] type=0x%02X(%s) value=0x%08X aux=0x%06X\n",
    ctx->logged_calls + 1,
    call_site,
    index,
    e->type,
    MVM_GetVmgpPoolTypeName(e->type),
    e->value,
    e->aux24);
  }

  ctx->logged_calls++;
} /* End of log_vm_call */

/**********************************************************************************************************************
 *  Name: log_syscall
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static void log_syscall(VMGPContext *ctx, uint8_t op)
{
  uint32_t argc = (op == OP_SYSCALL0) ? 0u : (uint32_t)(op - OP_SYSCALL0);
  MVM_Logf(ctx,
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
} /* End of log_syscall */

/**********************************************************************************************************************
 *  END OF FILE MVM_PipExec.c
 *********************************************************************************************************************/
