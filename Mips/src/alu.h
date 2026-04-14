#ifndef ALU_H
#define ALU_H

#include "mips.h"

/*
 * ALU 연산 코드 — 내부적으로 CPU 가 결정하여 alu_execute() 에 전달
 */
typedef enum {
    ALU_ADD,
    ALU_ADDU,
    ALU_SUB,
    ALU_SUBU,
    ALU_AND,
    ALU_OR,
    ALU_XOR,
    ALU_NOR,
    ALU_SLT,
    ALU_SLTU,
    ALU_SLL,
    ALU_SRL,
    ALU_SRA,
    ALU_LUI,
    ALU_PASS_B,   /* B 피연산자를 그대로 통과 (주소 계산 등) */
    ALU_NOP,
} ALUOp;

/*
 * ALU 연산 수행
 *  a     : 첫 번째 피연산자 (rs 값)
 *  b     : 두 번째 피연산자 (rt 값 또는 sign-extend(imm))
 *  shamt : 시프트 양 (R-type shift 명령어)
 *  op    : 연산 종류
 *  result: 연산 결과 (출력)
 *  zero  : a == b 이면 1, 아니면 0 (branch 판별용)
 */
void alu_execute(uint32_t a, uint32_t b, uint32_t shamt,
                 ALUOp op,
                 uint32_t *result, int *zero);

/*
 * 명령어 디코딩 결과에서 ALUOp 를 결정하는 헬퍼
 */
ALUOp alu_control(const DecodedInstr *d);

#endif /* ALU_H */