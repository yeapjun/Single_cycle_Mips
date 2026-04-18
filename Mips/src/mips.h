#ifndef MIPS_H
#define MIPS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================
 *  메모리 레이아웃 / 레지스터 초기값
 * ========================================================= */
#define MEM_SIZE        (0x2000000)   /* 32 MB */
#define LOAD_ADDR       (0x00000000)  /* 바이너리 로드 시작 주소 */
#define INIT_SP         (0x01000000)  /* $sp (r29) 초기값 */
#define INIT_LR         (0xFFFFFFFF)  /* $ra (r31) 초기값 — halt 주소 */
#define HALT_ADDR       (0xFFFFFFFF)  /* PC 가 이 값이 되면 종료 */
#define INIT_PC         (0x00000000)  /* PC 초기값 */

/* =========================================================
 *  레지스터 번호 별명 (필수 레지스터만 정의)
 * ========================================================= */
#define REG_ZERO  0   /* $zero — 상수 0 (쓰기 금지) */
#define REG_V0    2   /* $v0   — 반환값 출력용 */
#define REG_SP    29  /* $sp   — 스택 포인터 (초기화) */
#define REG_RA    31  /* $ra   — 복귀 주소 (JAL) */
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
#define INSTR_UIMM16(instr)   ((uint16_t)((instr) & 0xFFFFU))  /* 비부호 */
#define INSTR_TARGET(instr)   ((instr) & 0x03FFFFFFU)

/* =========================================================
 *  Opcode 상수 (테스트 프로그램에서 실제 사용되는 명령어만)
 * ========================================================= */
typedef enum {
    OP_RTYPE  = 0x00,  /* R-type (funct 로 구분) */
    OP_J      = 0x02,  /* 무조건 점프 */
    OP_JAL    = 0x03,  /* 서브루틴 호출 (r31 저장) */
    OP_BEQ    = 0x04,  /* 같으면 분기 */
    OP_BNE    = 0x05,  /* 다르면 분기 */
    OP_ADDIU  = 0x09,  /* 즉시값 덧셈 (비부호) */
    OP_SLTI   = 0x0A,  /* 즉시값 부호 비교 */
    OP_ORI    = 0x0D,  /* 즉시값 OR */
    OP_LUI    = 0x0F,  /* 상위 16비트 로드 */
    OP_LW     = 0x23,  /* 워드 로드 */
    OP_SW     = 0x2B,  /* 워드 스토어 */
} Opcode;

/* =========================================================
 *  R-type funct 코드 (테스트 프로그램에서 실제 사용되는 것만)
 * ========================================================= */
typedef enum {
    FUNCT_SLL   = 0x00,  /* 논리 좌시프트 */
    FUNCT_JR    = 0x08,  /* 레지스터 점프 */
    FUNCT_ADDU  = 0x21,  /* 비부호 덧셈 */
    FUNCT_SUBU  = 0x23,  /* 비부호 뺄셈 */
    FUNCT_SLT   = 0x2A,  /* 부호 있는 비교 */
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
    uint32_t  raw;         /* 원본 32-bit 명령어 워드 */
    uint32_t  pc;          /* 이 명령어의 PC */
    InstrType type;        /* R / I / J */

    /* 공통 필드 */
    uint32_t  opcode;
    uint32_t  rs, rt, rd;
    uint32_t  shamt;
    uint32_t  funct;
    int32_t   imm;         /* 부호 확장 16-bit immediate */
    uint32_t  uimm;        /* 비부호 16-bit immediate */
    uint32_t  target;      /* J-type 26-bit target */

    /* 제어 신호 */
    int  RegDst;    /* 1=rd, 0=rt 가 목적 레지스터 */
    int  ALUSrc;    /* 1=immediate, 0=register */
    int  MemRead;   /* LW */
    int  MemWrite;  /* SW */
    int  RegWrite;  /* 레지스터 쓰기 여부 */
    int  MemtoReg;  /* 1=메모리→레지스터, 0=ALU→레지스터 */
    int  IsBranch;  /* BEQ/BNE */
    int  IsJump;    /* J / JAL / JR */
    int  Link;      /* JAL → r31 저장 */
    int  jalr;      /* JALR (optional) */

    /* 실행 단계 중간값 */
    uint32_t  rs_val;  /* 레지스터 파일에서 읽은 rs 값 */
    uint32_t  rt_val;  /* 레지스터 파일에서 읽은 rt 값 */
} DecodedInstr;

/* =========================================================
 *  CPU 아키텍처 상태
 * ========================================================= */
typedef struct {
    uint32_t  reg[NUM_REGS];
    uint32_t  pc;
    uint32_t  hi, lo;       /* HI/LO (MULT/DIV 결과) */
    uint8_t  *mem;          /* 메모리 포인터 (동적 할당) */
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
    uint64_t mem_access;   /* lw/sw */
    uint64_t branches;     /* 분기 명령어 총 수 */
    uint64_t taken;        /* 실제 점프가 발생한 분기 수 */
} ExecStats;

#endif /* MIPS_H */
