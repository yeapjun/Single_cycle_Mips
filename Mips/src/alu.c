#include "alu.h"

void alu_execute(uint32_t a, uint32_t b, uint32_t shamt,
                 ALUOp op, uint32_t *result, int *zero)
{
    uint32_t res = 0;

    switch (op) {
    case ALU_ADD:    res = a + b;                                break;
    case ALU_SUB:    res = a - b;                                break;
    case ALU_OR:     res = a | b;                                break;
    case ALU_SLT:    res = ((int32_t)a < (int32_t)b) ? 1u : 0u; break;
    case ALU_SLL:    res = b << (shamt & 0x1F);                  break;
    case ALU_LUI:    res = b << 16;                              break;
    case ALU_PASS_B: res = b;                                    break;
    default:         res = 0;                                    break;
    }

    *result = res;
    *zero   = (a == b) ? 1 : 0;
}

ALUOp alu_control(const DecodedInstr *d)
{
    if (d->type == ITYPE_R) {
        switch ((Funct)d->funct) {
        case FUNCT_ADDU: return ALU_ADD;
        case FUNCT_SUBU: return ALU_SUB;
        case FUNCT_SLT:  return ALU_SLT;
        case FUNCT_SLL:  return ALU_SLL;
        case FUNCT_JR:   return ALU_PASS_B;
        default:         return ALU_NOP;
        }
    }

    switch ((Opcode)d->opcode) {
    case OP_ADDIU:            return ALU_ADD;
    case OP_SLTI:             return ALU_SLT;
    case OP_ORI:              return ALU_OR;
    case OP_LUI:              return ALU_LUI;
    case OP_LW:  case OP_SW:  return ALU_ADD;
    case OP_BEQ: case OP_BNE: return ALU_SUB;
    default:                  return ALU_NOP;
    }
}
