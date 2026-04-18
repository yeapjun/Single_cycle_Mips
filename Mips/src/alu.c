#include "alu.h"

/* ----------------------------------------------------------
 *  alu_execute: 실제 연산
 * ---------------------------------------------------------- */
void alu_execute(uint32_t a, uint32_t b, uint32_t shamt,
                 ALUOp op,
                 uint32_t *result, int *zero)
{
    uint32_t res = 0;

    switch (op) {
    case ALU_ADD:
        res = a + b;
        break;
    case ALU_SUB:
        res = a - b;
        break;
    case ALU_OR:
        res = a | b;
        break;
    case ALU_SLT:
        /* 부호 있는 비교 */
        res = ((int32_t)a < (int32_t)b) ? 1u : 0u;
        break;
    case ALU_SLL:
        /* SLL: b << shamt */
        res = b << (shamt & 0x1F);
        break;
    case ALU_LUI:
        /* 상위 16비트에 즉시값 배치 */
        res = b << 16;
        break;
    case ALU_PASS_B:
        res = b;
        break;
    case ALU_NOP:
    default:
        res = 0;
        break;
    }

    *result = res;
    *zero   = (a == b) ? 1 : 0;   /* BEQ/BNE 판별용 */
}

/* ----------------------------------------------------------
 *  alu_control: DecodedInstr → ALUOp 결정
 * ---------------------------------------------------------- */
ALUOp alu_control(const DecodedInstr *d)
{
    if (d->type == ITYPE_R) {
        switch ((Funct)d->funct) {
        case FUNCT_ADDU:  return ALU_ADD;
        case FUNCT_SUBU:  return ALU_SUB;
        case FUNCT_SLT:   return ALU_SLT;
        case FUNCT_SLL:   return ALU_SLL;
        case FUNCT_JR:    return ALU_PASS_B;
        default:          return ALU_NOP;
        }
    }

    /* I-type / J-type */
    switch ((Opcode)d->opcode) {
    case OP_ADDIU:
        return ALU_ADD;
    case OP_SLTI:
        return ALU_SLT;
    case OP_ORI:
        return ALU_OR;
    case OP_LUI:
        return ALU_LUI;
    /* 로드/스토어: 주소 = rs + sign_ext(imm) */
    case OP_LW:
    case OP_SW:
        return ALU_ADD;
    /* 분기: rs - rt 로 zero 플래그 사용 */
    case OP_BEQ:
    case OP_BNE:
        return ALU_SUB;
    default:
        return ALU_NOP;
    }
}
