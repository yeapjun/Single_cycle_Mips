#ifndef MIPS_H
#define MIPS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================
 *  메모리 레이아웃 / 레지스터 초기값
 * ========================================================= */
#define MEM_SIZE        (0x2000000)   /* 32 MB — stack + text 공간 여유 */
#define LOAD_ADDR       (0x00000000)  /* 바이너리 로드 시작 주소        */
#define INIT_SP         (0x01000000)  /* $sp (r29) 초기값               */
#define INIT_LR         (0xFFFFFFFF)  /* $ra (r31) 초기값 — halt 주소   */
#define HALT_ADDR       (0xFFFFFFFF)  /* PC 가 이 값이 되면 종료        */
#define INIT_PC         (0x00000000)  /* PC 초기값                      */

/* =========================================================
 *  레지스터 번호 별명 (MIPS 컨벤션)
 * ========================================================= */
#define REG_ZERO  0
#define REG_AT    1
#define REG_V0    2   /* 반환값 */
#define REG_V1    3
#define REG_A0    4
#define REG_A1    5
#define REG_A2    6
#define REG_A3    7
#define REG_T0    8
#define REG_T1    9
#define REG_T2    10
#define REG_T3    11
#define REG_T4    12
#define REG_T5    13
#define REG_T6    14
#define REG_T7    15
#define REG_S0    16
#define REG_S1    17
#define REG_S2    18
#define REG_S3    19
#define REG_S4    20
#define REG_S5    21
#define REG_S6    22
#define REG_S7    23
#define REG_T8    24
#define REG_T9    25
#define REG_K0    26
#define REG_K1    27
#define REG_GP    28
#define REG_SP    29
#define REG_FP    30
#define REG_RA    31
#define NUM_REGS  32

/* =========================================================
 *  명령어 필드 추출 매크로
 *  MIPS 32-bit 명령어 형식:
 *    R : [31:26]=opcode [25:21]=rs [20:16]=rt [15:11]=rd [10:6]=shamt [5:0]=funct
 *    I : [31:26]=opcode [25:21]=rs [20:16]=rt [15:0]=imm16
 *    J : [31:26]=opcode [25:0]=target26
 * ========================================================= */
#define INSTR_OPCODE(instr)   (((instr) >> 26) & 0x3FU)
#define INSTR_RS(instr)       (((instr) >> 21) & 0x1FU)
#define INSTR_RT(instr)       (((instr) >> 16) & 0x1FU)
#define INSTR_RD(instr)       (((instr) >> 11) & 0x1FU)
#define INSTR_SHAMT(instr)    (((instr) >>  6) & 0x1FU)
#define INSTR_FUNCT(instr)    (((instr)      ) & 0x3FU)
#define INSTR_IMM16(instr)    ((int16_t)((instr) & 0xFFFFU))   /* 부호 확장 */
#define INSTR_UIMM16(instr)   ((uint16_t)((instr) & 0xFFFFU))  /* 비부호    */
#define INSTR_TARGET(instr)   ((instr) & 0x03FFFFFFU)

/* =========================================================
 *  Opcode 상수
 * ========================================================= */
typedef enum {
    OP_RTYPE  = 0x00,  /* R-type (funct 로 구분) */
    OP_J      = 0x02,
    OP_JAL    = 0x03,
    OP_BEQ    = 0x04,
    OP_BNE    = 0x05,
    OP_BLEZ   = 0x06,
    OP_BGTZ   = 0x07,
    OP_ADDI   = 0x08,
    OP_ADDIU  = 0x09,
    OP_SLTI   = 0x0A,
    OP_SLTIU  = 0x0B,
    OP_ANDI   = 0x0C,
    OP_ORI    = 0x0D,
    OP_XORI   = 0x0E,
    OP_LUI    = 0x0F,
    OP_LB     = 0x20,
    OP_LH     = 0x21,
    OP_LW     = 0x23,
    OP_LBU    = 0x24,
    OP_LHU    = 0x25,
    OP_SB     = 0x28,
    OP_SH     = 0x29,
    OP_SW     = 0x2B,
} Opcode;

/* =========================================================
 *  R-type funct 코드
 * ========================================================= */
typedef enum {
    FUNCT_SLL   = 0x00,
    FUNCT_SRL   = 0x02,
    FUNCT_SRA   = 0x03,
    FUNCT_SLLV  = 0x04,
    FUNCT_SRLV  = 0x06,
    FUNCT_SRAV  = 0x07,
    FUNCT_JR    = 0x08,
    FUNCT_JALR  = 0x09,  /* optional: JALR */
    FUNCT_MFHI  = 0x10,
    FUNCT_MTHI  = 0x11,
    FUNCT_MFLO  = 0x12,
    FUNCT_MTLO  = 0x13,
    FUNCT_MULT  = 0x18,
    FUNCT_MULTU = 0x19,
    FUNCT_DIV   = 0x1A,
    FUNCT_DIVU  = 0x1B,
    FUNCT_ADD   = 0x20,
    FUNCT_ADDU  = 0x21,
    FUNCT_SUB   = 0x22,
    FUNCT_SUBU  = 0x23,
    FUNCT_AND   = 0x24,
    FUNCT_OR    = 0x25,
    FUNCT_XOR   = 0x26,
    FUNCT_NOR   = 0x27,
    FUNCT_SLT   = 0x2A,
    FUNCT_SLTU  = 0x2B,
} Funct;

/* =========================================================
 *  명령어 타입
 * ========================================================= */
typedef enum {
    ITYPE_R,
    ITYPE_I,
    ITYPE_J,
    ITYPE_UNKNOWN
} InstrType;

/* =========================================================
 *  디코딩 결과 구조체
 * ========================================================= */
typedef struct {
    uint32_t  raw;         /* 원본 32-bit 명령어 워드       */
    uint32_t  pc;          /* 이 명령어의 PC                */
    InstrType type;        /* R / I / J                     */

    /* 공통 필드 */
    uint32_t  opcode;
    uint32_t  rs, rt, rd;
    uint32_t  shamt;
    uint32_t  funct;
    int32_t   imm;         /* 부호 확장 16-bit immediate    */
    uint32_t  uimm;        /* 비부호 16-bit immediate       */
    uint32_t  target;      /* J-type 26-bit target          */

    /* 제어 신호 (디코드 단계에서 설정) */
    int  reg_dst;    /* 1=rd, 0=rt 가 목적 레지스터        */
    int  alu_src;    /* 1=immediate, 0=register            */
    int  mem_read;   /* LW/LH/LB 등                        */
    int  mem_write;  /* SW/SH/SB 등                        */
    int  reg_write;  /* 레지스터 쓰기 여부                 */
    int  mem_to_reg; /* 1=메모리→레지스터, 0=ALU→레지스터  */
    int  is_branch;  /* BEQ/BNE/BLEZ/BGTZ                  */
    int  is_jump;    /* J / JAL / JR / JALR                */
    int  link;       /* JAL / JALR → r31 저장              */
    int  jalr;       /* JALR optional instruction          */

    /* 실행 단계 중간값 */
    uint32_t  rs_val;  /* 레지스터 파일에서 읽은 rs 값     */
    uint32_t  rt_val;  /* 레지스터 파일에서 읽은 rt 값     */
} DecodedInstr;

/* =========================================================
 *  CPU 아키텍처 상태
 * ========================================================= */
typedef struct {
    uint32_t  reg[NUM_REGS];
    uint32_t  pc;
    uint32_t  hi, lo;       /* HI/LO (MULT/DIV 결과)         */
    uint8_t  *mem;          /* 메모리 포인터 (동적 할당)      */
    uint32_t  mem_size;
} CPUState;

/* =========================================================
 *  실행 통계
 * ========================================================= */
typedef struct {
    uint64_t total;
    uint64_t r_type;
    uint64_t i_type;
    uint64_t j_type;
    uint64_t mem_access;   /* lw/lh/lb/lbu/lhu/sw/sh/sb     */
    uint64_t branches;     /* 분기 명령어 총 수              */
    uint64_t taken;        /* 실제 점프가 발생한 분기 수     */
} ExecStats;

#endif /* MIPS_H */