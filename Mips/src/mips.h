#ifndef MIPS_H
#define MIPS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE    (0x2000000)
#define LOAD_ADDR   (0x00000000)
#define INIT_SP     (0x01000000)
#define INIT_LR     (0xFFFFFFFF)
#define HALT_ADDR   (0xFFFFFFFF)
#define INIT_PC     (0x00000000)

#define REG_ZERO  0
#define REG_V0    2
#define REG_SP    29
#define REG_RA    31
#define NUM_REGS  32

/* R: [31:26]=op [25:21]=rs [20:16]=rt [15:11]=rd [10:6]=shamt [5:0]=funct
 * I: [31:26]=op [25:21]=rs [20:16]=rt [15:0]=imm16
 * J: [31:26]=op [25:0]=target26 */
#define INSTR_OPCODE(instr)   (((instr) >> 26) & 0x3FU)
#define INSTR_RS(instr)       (((instr) >> 21) & 0x1FU)
#define INSTR_RT(instr)       (((instr) >> 16) & 0x1FU)
#define INSTR_RD(instr)       (((instr) >> 11) & 0x1FU)
#define INSTR_SHAMT(instr)    (((instr) >>  6) & 0x1FU)
#define INSTR_FUNCT(instr)    (((instr)      ) & 0x3FU)
#define INSTR_IMM16(instr)    ((int16_t)((instr)  & 0xFFFFU))
#define INSTR_UIMM16(instr)   ((uint16_t)((instr) & 0xFFFFU))
#define INSTR_TARGET(instr)   ((instr) & 0x03FFFFFFU)

typedef enum {
    OP_RTYPE = 0x00,
    OP_J     = 0x02,
    OP_JAL   = 0x03,
    OP_BEQ   = 0x04,
    OP_BNE   = 0x05,
    OP_ADDIU = 0x09,
    OP_SLTI  = 0x0A,
    OP_ORI   = 0x0D,
    OP_LUI   = 0x0F,
    OP_LW    = 0x23,
    OP_SW    = 0x2B,
} Opcode;

typedef enum {
    FUNCT_SLL  = 0x00,
    FUNCT_JR   = 0x08,
    FUNCT_ADDU = 0x21,
    FUNCT_SUBU = 0x23,
    FUNCT_SLT  = 0x2A,
} Funct;

typedef enum {
    ITYPE_R,
    ITYPE_I,
    ITYPE_J,
    ITYPE_UNKNOWN
} InstrType;

typedef struct {
    uint32_t  raw;
    uint32_t  pc;
    InstrType type;

    uint32_t  opcode;
    uint32_t  rs, rt, rd;
    uint32_t  shamt;
    uint32_t  funct;
    int32_t   imm;    /* sign-extended */
    uint32_t  uimm;   /* zero-extended */
    uint32_t  target;

    int  RegDst;
    int  ALUSrc;
    int  MemRead;
    int  MemWrite;
    int  RegWrite;
    int  MemtoReg;
    int  IsBranch;
    int  IsJump;
    int  Link;
    int  jalr;

    uint32_t  rs_val;
    uint32_t  rt_val;
} DecodedInstr;

typedef struct {
    uint32_t  reg[NUM_REGS];
    uint32_t  pc;
    uint32_t  hi, lo;
    uint8_t  *mem;
    uint32_t  mem_size;
} CPUState;

typedef struct {
    uint64_t total;
    uint64_t r_type;
    uint64_t i_type;
    uint64_t j_type;
    uint64_t mem_access;
    uint64_t branches;
    uint64_t taken;
} ExecStats;

#endif /* MIPS_H */
