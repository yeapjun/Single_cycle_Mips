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
    case ALU_ADDU:
        res = a + b;
        break;
    case ALU_SUB:
    case ALU_SUBU:
        res = a - b;
        break;
    case ALU_AND:
        res = a & b;
        break;
    case ALU_OR:
        res = a | b;
        break;
    case ALU_XOR:
        res = a ^ b;
        break;
    case ALU_NOR:
        res = ~(a | b);
        break;
    case ALU_SLT:
        /* 부호 있는 비교 */
        res = ((int32_t)a < (int32_t)b) ? 1u : 0u;
        break;
    case ALU_SLTU:
        /* 비부호 비교 */
        res = (a < b) ? 1u : 0u;
        break;
    case ALU_SLL:
        /* b == shamt (R-type 시프트) 또는 SLLV 일 때는 a 의 하위 5비트 */
        res = b << (shamt & 0x1F);
        break;
    case ALU_SRL:
        res = b >> (shamt & 0x1F);
        break;
    case ALU_SRA:
        res = (uint32_t)((int32_t)b >> (shamt & 0x1F));
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
        case FUNCT_ADD:   return ALU_ADD;
        case FUNCT_ADDU:  return ALU_ADDU;
        case FUNCT_SUB:   return ALU_SUB;
        case FUNCT_SUBU:  return ALU_SUBU;
        case FUNCT_AND:   return ALU_AND;
        case FUNCT_OR:    return ALU_OR;
        case FUNCT_XOR:   return ALU_XOR;
        case FUNCT_NOR:   return ALU_NOR;
        case FUNCT_SLT:   return ALU_SLT;
        case FUNCT_SLTU:  return ALU_SLTU;
        case FUNCT_SLL:   return ALU_SLL;
        case FUNCT_SRL:   return ALU_SRL;
        case FUNCT_SRA:   return ALU_SRA;
        case FUNCT_SLLV:  return ALU_SLL;
        case FUNCT_SRLV:  return ALU_SRL;
        case FUNCT_SRAV:  return ALU_SRA;
        /* JR/JALR: 주소 계산은 CPU 에서 직접 처리 */
        case FUNCT_JR:
        case FUNCT_JALR:  return ALU_PASS_B;
        /* HI/LO 이동, 곱셈/나눗셈: CPU 에서 직접 처리 */
        default:          return ALU_NOP;
        }
    }

    /* I-type / J-type */
    switch ((Opcode)d->opcode) {
    case OP_ADDI:
    case OP_ADDIU:
        return ALU_ADD;
    case OP_SLTI:
        return ALU_SLT;
    case OP_SLTIU:
        return ALU_SLTU;
    case OP_ANDI:
        return ALU_AND;
    case OP_ORI:
        return ALU_OR;
    case OP_XORI:
        return ALU_XOR;
    case OP_LUI:
        return ALU_LUI;
    /* 로드/스토어: 주소 = rs + sign_ext(imm) */
    case OP_LB: case OP_LH: case OP_LW:
    case OP_LBU: case OP_LHU:
    case OP_SB: case OP_SH: case OP_SW:
        return ALU_ADD;
    /* 분기: rs - rt 로 zero 플래그만 사용 */
    case OP_BEQ:
    case OP_BNE:
        return ALU_SUB;
    case OP_BLEZ:
    case OP_BGTZ:
        return ALU_SUB;
    default:
        return ALU_NOP;
    }
}