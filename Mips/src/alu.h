#ifndef ALU_H
#define ALU_H

#include "mips.h"

/*
 * ALU 연산 코드 — CPU 가 결정하여 alu_execute() 에 전달
 * (테스트 프로그램에서 실제 사용되는 연산만 정의)
 */
typedef enum {
    ALU_ADD,      /* 덧셈 (ADDU, ADDIU, LW/SW 주소 계산) */
    ALU_SUB,      /* 뺄셈 (SUBU, BEQ/BNE 비교) */
    ALU_OR,       /* 논리 OR (ORI) */
    ALU_SLT,      /* 부호 있는 비교 (SLT, SLTI) */
    ALU_SLL,      /* 논리 좌시프트 (SLL) */
    ALU_LUI,      /* 상위 16비트 로드 (LUI) */
    ALU_PASS_B,   /* B 피연산자 통과 (JR 주소 전달) */
    ALU_NOP,
} ALUOp;

/*
 * ALU 연산 수행
 *  a     : 첫 번째 피연산자 (rs 값)
 *  b     : 두 번째 피연산자 (rt 값 또는 sign-extend(imm))
 *  shamt : 시프트 양 (SLL)
 *  op    : 연산 종류
 *  result: 연산 결과 (출력)
 *  zero  : a == b 이면 1, 아니면 0 (BEQ/BNE 판별용)
 */
void alu_execute(uint32_t a, uint32_t b, uint32_t shamt,
                 ALUOp op,
                 uint32_t *result, int *zero);

/*
 * 명령어 디코딩 결과에서 ALUOp 를 결정하는 헬퍼
 */
ALUOp alu_control(const DecodedInstr *d);

#endif /* ALU_H */
