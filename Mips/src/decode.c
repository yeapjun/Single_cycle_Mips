#include "decode.h"

/* ----------------------------------------------------------
 *  제어 신호 초기화 헬퍼
 * ---------------------------------------------------------- */
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

/* ----------------------------------------------------------
 *  R-type 제어 신호 설정
 * ---------------------------------------------------------- */
static void ctrl_rtype(DecodedInstr *d)
{
    switch ((Funct)d->funct) {
    /* 산술/논리/시프트: 결과를 rd 에 기록 */
    case FUNCT_ADDU:
    case FUNCT_SUBU:
    case FUNCT_SLT:
    case FUNCT_SLL:
        d->RegDst  = 1;
        d->RegWrite= 1;
        break;

    /* 점프 */
    case FUNCT_JR:
        d->IsJump  = 1;
        break;

    default:
        fprintf(stderr, "[WARN] Unknown R-type funct: 0x%02X at PC=0x%08X\n",
                d->funct, d->pc);
        break;
    }
}

/* ----------------------------------------------------------
 *  decode_instruction: 메인 디코딩 함수
 * ---------------------------------------------------------- */
void decode_instruction(const CPUState *cpu, uint32_t raw,
                        uint32_t pc, DecodedInstr *d)
{
    memset(d, 0, sizeof(DecodedInstr));
    d->raw    = raw;
    d->pc     = pc;

    /* 필드 추출 */
    d->opcode = INSTR_OPCODE(raw);
    d->rs     = INSTR_RS(raw);
    d->rt     = INSTR_RT(raw);
    d->rd     = INSTR_RD(raw);
    d->shamt  = INSTR_SHAMT(raw);
    d->funct  = INSTR_FUNCT(raw);
    d->imm    = (int32_t)INSTR_IMM16(raw);   /* 부호 확장 */
    d->uimm   = (uint32_t)INSTR_UIMM16(raw);
    d->target = INSTR_TARGET(raw);

    ctrl_clear(d);

    /* 레지스터 파일 읽기 ($0 는 항상 0) */
    d->rs_val = cpu->reg[d->rs];
    d->rt_val = cpu->reg[d->rt];

    /* -------------------------------------------------------
     *  타입 분류 및 제어 신호 설정
     * ------------------------------------------------------- */
    switch ((Opcode)d->opcode) {

    /* ---- R-type ---- */
    case OP_RTYPE:
        d->type = ITYPE_R;
        ctrl_rtype(d);
        break;

    /* ---- J-type ---- */
    case OP_J:
        d->type    = ITYPE_J;
        d->IsJump  = 1;
        break;
    case OP_JAL:
        d->type     = ITYPE_J;
        d->IsJump   = 1;
        d->Link     = 1;
        d->RegWrite = 1;  /* r31 에 PC+8 저장 */
        break;

    /* ---- I-type: 분기 ---- */
    case OP_BEQ:
    case OP_BNE:
        d->type     = ITYPE_I;
        d->IsBranch = 1;
        break;

    /* ---- I-type: 산술/논리 즉시값 ---- */
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

    /* ---- I-type: 로드 ---- */
    case OP_LW:
        d->type     = ITYPE_I;
        d->ALUSrc   = 1;
        d->MemRead  = 1;
        d->RegWrite = 1;
        d->MemtoReg = 1;
        break;

    /* ---- I-type: 스토어 ---- */
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
