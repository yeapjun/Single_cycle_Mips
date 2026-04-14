#include "decode.h"

/* ----------------------------------------------------------
 *  제어 신호 초기화 헬퍼
 * ---------------------------------------------------------- */
static void ctrl_clear(DecodedInstr *d)
{
    d->reg_dst   = 0;
    d->alu_src   = 0;
    d->mem_read  = 0;
    d->mem_write = 0;
    d->reg_write = 0;
    d->mem_to_reg= 0;
    d->is_branch = 0;
    d->is_jump   = 0;
    d->link      = 0;
    d->jalr      = 0;
}

/* ----------------------------------------------------------
 *  R-type 제어 신호 설정
 * ---------------------------------------------------------- */
static void ctrl_rtype(DecodedInstr *d)
{
    switch ((Funct)d->funct) {
    /* 산술/논리/시프트: 결과를 rd 에 기록 */
    case FUNCT_ADD:  case FUNCT_ADDU:
    case FUNCT_SUB:  case FUNCT_SUBU:
    case FUNCT_AND:  case FUNCT_OR:
    case FUNCT_XOR:  case FUNCT_NOR:
    case FUNCT_SLT:  case FUNCT_SLTU:
    case FUNCT_SLL:  case FUNCT_SRL:  case FUNCT_SRA:
    case FUNCT_SLLV: case FUNCT_SRLV: case FUNCT_SRAV:
        d->reg_dst  = 1;
        d->reg_write= 1;
        break;

    /* HI/LO 이동: 레지스터 쓰기 필요 */
    case FUNCT_MFHI: case FUNCT_MFLO:
        d->reg_dst  = 1;
        d->reg_write= 1;
        break;
    case FUNCT_MTHI: case FUNCT_MTLO:
        /* HI/LO 레지스터 업데이트 — reg_write 불필요(GPR 안 씀) */
        break;

    /* 곱셈/나눗셈: HI/LO 업데이트 */
    case FUNCT_MULT: case FUNCT_MULTU:
    case FUNCT_DIV:  case FUNCT_DIVU:
        break;

    /* 점프 */
    case FUNCT_JR:
        d->is_jump  = 1;
        break;
    case FUNCT_JALR:  /* optional */
        d->is_jump  = 1;
        d->link     = 1;
        d->jalr     = 1;
        d->reg_dst  = 1;   /* rd(=31 관례) 에 저장 */
        d->reg_write= 1;
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
        d->is_jump = 1;
        break;
    case OP_JAL:
        d->type     = ITYPE_J;
        d->is_jump  = 1;
        d->link     = 1;
        d->reg_write= 1;  /* r31 에 PC+8 저장 */
        break;

    /* ---- I-type: 분기 ---- */
    case OP_BEQ:
    case OP_BNE:
        d->type      = ITYPE_I;
        d->is_branch = 1;
        break;
    case OP_BLEZ:
    case OP_BGTZ:
        d->type      = ITYPE_I;
        d->is_branch = 1;
        break;

    /* ---- I-type: 산술/논리 즉시값 ---- */
    case OP_ADDI:
    case OP_ADDIU:
    case OP_SLTI:
    case OP_SLTIU:
        d->type      = ITYPE_I;
        d->alu_src   = 1;
        d->reg_write = 1;
        break;
    case OP_ANDI:
    case OP_ORI:
    case OP_XORI:
        d->type      = ITYPE_I;
        d->alu_src   = 1;
        d->reg_write = 1;
        break;
    case OP_LUI:
        d->type      = ITYPE_I;
        d->alu_src   = 1;
        d->reg_write = 1;
        break;

    /* ---- I-type: 로드 ---- */
    case OP_LB:
    case OP_LH:
    case OP_LW:
    case OP_LBU:
    case OP_LHU:
        d->type       = ITYPE_I;
        d->alu_src    = 1;
        d->mem_read   = 1;
        d->reg_write  = 1;
        d->mem_to_reg = 1;
        break;

    /* ---- I-type: 스토어 ---- */
    case OP_SB:
    case OP_SH:
    case OP_SW:
        d->type      = ITYPE_I;
        d->alu_src   = 1;
        d->mem_write = 1;
        break;

    default:
        d->type = ITYPE_UNKNOWN;
        fprintf(stderr, "[WARN] Unknown opcode: 0x%02X at PC=0x%08X\n",
                d->opcode, d->pc);
        break;
    }
}

