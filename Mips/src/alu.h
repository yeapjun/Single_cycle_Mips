#ifndef ALU_H
#define ALU_H

#include "mips.h"

typedef enum {
    ALU_ADD,
    ALU_SUB,
    ALU_OR,
    ALU_SLT,
    ALU_SLL,
    ALU_LUI,
    ALU_PASS_B,
    ALU_NOP,
} ALUOp;

void  alu_execute(uint32_t a, uint32_t b, uint32_t shamt,
                  ALUOp op, uint32_t *result, int *zero);
ALUOp alu_control(const DecodedInstr *d);

#endif /* ALU_H */
