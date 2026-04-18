#include "decode.h"

static void ctrl_clear(DecodedInstr *d)
{
    d->RegDst   = 0;
    d->ALUSrc   = 0;
    d->MemRead  = 0;
    d->MemWrite = 0;
    d->RegWrite = 0;
    d->MemtoReg = 0;
    d->IsBranch = 0;
    d->IsJump   = 0;
    d->Link     = 0;
    d->jalr     = 0;
}

static void ctrl_rtype(DecodedInstr *d)
{
    switch ((Funct)d->funct) {
    case FUNCT_ADDU:
    case FUNCT_SUBU:
    case FUNCT_SLT:
    case FUNCT_SLL:
        d->RegDst  = 1;
        d->RegWrite = 1;
        break;
    case FUNCT_JR:
        d->IsJump = 1;
        break;
    default:
        fprintf(stderr, "[WARN] Unknown R-type funct: 0x%02X at PC=0x%08X\n",
                d->funct, d->pc);
        break;
    }
}

void decode_instruction(const CPUState *cpu, uint32_t raw,
                        uint32_t pc, DecodedInstr *d)
{
    memset(d, 0, sizeof(DecodedInstr));
    d->raw    = raw;
    d->pc     = pc;

    d->opcode = INSTR_OPCODE(raw);
    d->rs     = INSTR_RS(raw);
    d->rt     = INSTR_RT(raw);
    d->rd     = INSTR_RD(raw);
    d->shamt  = INSTR_SHAMT(raw);
    d->funct  = INSTR_FUNCT(raw);
    d->imm    = (int32_t)INSTR_IMM16(raw);
    d->uimm   = (uint32_t)INSTR_UIMM16(raw);
    d->target = INSTR_TARGET(raw);

    ctrl_clear(d);

    d->rs_val = cpu->reg[d->rs];
    d->rt_val = cpu->reg[d->rt];

    switch ((Opcode)d->opcode) {

    case OP_RTYPE:
        d->type = ITYPE_R;
        ctrl_rtype(d);
        break;

    case OP_J:
        d->type   = ITYPE_J;
        d->IsJump = 1;
        break;
    case OP_JAL:
        d->type     = ITYPE_J;
        d->IsJump   = 1;
        d->Link     = 1;
        d->RegWrite = 1;
        break;

    case OP_BEQ:
    case OP_BNE:
        d->type     = ITYPE_I;
        d->IsBranch = 1;
        break;

    case OP_ADDIU:
    case OP_SLTI:
        d->type     = ITYPE_I;
        d->ALUSrc   = 1;
        d->RegWrite = 1;
        break;
    case OP_ORI:
        d->type     = ITYPE_I;
        d->ALUSrc   = 1;
        d->RegWrite = 1;
        break;
    case OP_LUI:
        d->type     = ITYPE_I;
        d->ALUSrc   = 1;
        d->RegWrite = 1;
        break;

    case OP_LW:
        d->type     = ITYPE_I;
        d->ALUSrc   = 1;
        d->MemRead  = 1;
        d->RegWrite = 1;
        d->MemtoReg = 1;
        break;

    case OP_SW:
        d->type     = ITYPE_I;
        d->ALUSrc   = 1;
        d->MemWrite = 1;
        break;

    default:
        d->type = ITYPE_UNKNOWN;
        fprintf(stderr, "[WARN] Unknown opcode: 0x%02X at PC=0x%08X\n",
                d->opcode, d->pc);
        break;
    }
}
