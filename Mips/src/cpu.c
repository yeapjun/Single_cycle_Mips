#include "cpu.h"

/* ================================================================
 *  CPU 초기화
 * ================================================================ */
void cpu_init(CPUState *cpu)
{
    memset(cpu->reg, 0, sizeof(cpu->reg));
    cpu->reg[REG_SP] = INIT_SP;
    cpu->reg[REG_RA] = INIT_LR;
    cpu->pc          = INIT_PC;
    cpu->hi          = 0;
    cpu->lo          = 0;
    /* $zero 는 항상 0 — 쓰기 금지는 write-back 단계에서 강제 */
}

/* ================================================================
 *  레지스터 이름 테이블 (출력용)
 * ================================================================ */
static const char *reg_name[NUM_REGS] = {
    "$zero","$at","$v0","$v1","$a0","$a1","$a2","$a3",
    "$t0","$t1","$t2","$t3","$t4","$t5","$t6","$t7",
    "$s0","$s1","$s2","$s3","$s4","$s5","$s6","$s7",
    "$t8","$t9","$k0","$k1","$gp","$sp","$fp","$ra"
};

/* ================================================================
 *  단계 1: Instruction Fetch
 *  - PC 가 가리키는 메모리에서 32-bit 명령어 로드
 *  - 다음 순차 PC(PC+4) 계산
 * ================================================================ */
static uint32_t stage_if(const CPUState *cpu)
{
    uint32_t instr = mem_read_word(cpu, cpu->pc);
    return instr;
}

/* ================================================================
 *  단계 2: Instruction Decode / Register File Read
 *  - 명령어 파싱 + 제어 신호 설정
 *  - rs, rt 레지스터 값 읽기
 * ================================================================ */
static void stage_id(const CPUState *cpu, uint32_t raw, uint32_t pc,
                     DecodedInstr *d)
{
    decode_instruction(cpu, raw, pc, d);
}

/* ================================================================
 *  단계 3: Execution (ALU + Branch/Jump 목적지 계산)
 *  반환값: ALU 연산 결과 (메모리 주소 or 레지스터 쓰기 값)
 * ================================================================ */
static uint32_t stage_ex(const CPUState *cpu, DecodedInstr *d,
                         uint32_t *next_pc, int *branch_taken)
{
    uint32_t alu_result = 0;
    int      zero       = 0;

    /* ---- ALU 피연산자 B 결정 ----------------------------------- */
    uint32_t op_a = d->rs_val;
    uint32_t op_b;

    /* SLLV/SRLV/SRAV: shamt = rs 의 하위 5비트 */
    uint32_t shamt = d->shamt;

    if (d->type == ITYPE_R) {
        switch ((Funct)d->funct) {
        case FUNCT_SLLV:
        case FUNCT_SRLV:
        case FUNCT_SRAV:
            op_b  = d->rt_val;
            shamt = d->rs_val & 0x1F;
            break;
        case FUNCT_SLL:
        case FUNCT_SRL:
        case FUNCT_SRA:
            op_b  = d->rt_val;
            /* shamt 는 d->shamt 그대로 */
            break;
        default:
            op_b = d->rt_val;
            break;
        }
    } else {
        /* I-type: 논리 연산 ANDI/ORI/XORI 는 zero-extend */
        if (d->opcode == OP_ANDI ||
            d->opcode == OP_ORI  ||
            d->opcode == OP_XORI) {
            op_b = (uint32_t)d->uimm;
        } else {
            op_b = (uint32_t)d->imm; /* 부호 확장 */
        }
    }

    /* ---- ALU 연산 ---------------------------------------------- */
    ALUOp aop = alu_control(d);

    /* MULT/DIV 는 별도 처리 */
    if (d->type == ITYPE_R &&
        (d->funct == FUNCT_MULT || d->funct == FUNCT_MULTU ||
         d->funct == FUNCT_DIV  || d->funct == FUNCT_DIVU)) {

        /* HI/LO 갱신은 write-back 에서 처리하도록
           여기선 0 반환 (result 는 사용 안 함) */
        alu_result = 0;
        zero = 0;
    } else {
        alu_execute(op_a, op_b, shamt, aop, &alu_result, &zero);
    }

    /* ---- 다음 PC 계산 ------------------------------------------ */
    uint32_t pc4 = d->pc + 4;  /* 순차 PC */

    *branch_taken = 0;
    *next_pc      = pc4;

    if (d->is_jump) {
        if (d->type == ITYPE_J) {
            /* J / JAL: {PC+4[31:28], target26, 00} */
            *next_pc = (pc4 & 0xF0000000u) | (d->target << 2);
        } else if (d->type == ITYPE_R) {
            /* JR / JALR: PC = rs */
            *next_pc = d->rs_val;
        }
        *branch_taken = 1;
    } else if (d->is_branch) {
        int take = 0;
        int32_t rs_s = (int32_t)d->rs_val;

        switch ((Opcode)d->opcode) {
        case OP_BEQ:  take = (d->rs_val == d->rt_val);          break;
        case OP_BNE:  take = (d->rs_val != d->rt_val);          break;
        case OP_BLEZ: take = (rs_s <= 0);                       break;
        case OP_BGTZ: take = (rs_s >  0);                       break;
        default:      take = 0;                                  break;
        }

        if (take) {
            /* Branch target = PC+4 + sign_ext(imm16)<<2 */
            *next_pc      = (uint32_t)((int32_t)pc4 + (d->imm << 2));
            *branch_taken = 1;
        }
    }

    (void)cpu; /* 현재 단계에서 CPUState 직접 읽기 없음 */
    return alu_result;
}

/* ================================================================
 *  단계 4: Memory Access (Load / Store)
 * ================================================================ */
static uint32_t stage_mem(CPUState *cpu, const DecodedInstr *d,
                           uint32_t alu_result, ExecStats *stats)
{
    uint32_t mem_data = 0;

    if (d->mem_read) {
        stats->mem_access++;
        switch ((Opcode)d->opcode) {
        case OP_LW:
            mem_data = mem_read_word(cpu, alu_result);
            break;
        case OP_LH:
            mem_data = (uint32_t)(int32_t)(int16_t)mem_read_half(cpu, alu_result);
            break;
        case OP_LHU:
            mem_data = (uint32_t)mem_read_half(cpu, alu_result);
            break;
        case OP_LB:
            mem_data = (uint32_t)(int32_t)(int8_t)mem_read_byte(cpu, alu_result);
            break;
        case OP_LBU:
            mem_data = (uint32_t)mem_read_byte(cpu, alu_result);
            break;
        default:
            break;
        }
    } else if (d->mem_write) {
        stats->mem_access++;
        switch ((Opcode)d->opcode) {
        case OP_SW:
            mem_write_word(cpu, alu_result, d->rt_val);
            break;
        case OP_SH:
            mem_write_half(cpu, alu_result, (uint16_t)(d->rt_val & 0xFFFF));
            break;
        case OP_SB:
            mem_write_byte(cpu, alu_result, (uint8_t)(d->rt_val & 0xFF));
            break;
        default:
            break;
        }
    }

    return mem_data;
}

/* ================================================================
 *  단계 5: Write Back
 *  - ALU 결과 또는 메모리 데이터를 레지스터에 저장
 *  - JAL/JALR 링크 레지스터 저장
 *  - MULT/DIV HI/LO 업데이트
 *  - PC 업데이트
 * ================================================================ */
static void stage_wb(CPUState *cpu, const DecodedInstr *d,
                     uint32_t alu_result, uint32_t mem_data,
                     uint32_t next_pc, int verbose)
{
    /* ---- 변경 상태 추적을 위해 이전 값 저장 ---- */
    uint32_t prev_reg[NUM_REGS];
    memcpy(prev_reg, cpu->reg, sizeof(cpu->reg));
    uint32_t prev_pc = cpu->pc;

    /* ---- MULT / DIV HI/LO 갱신 ---- */
    if (d->type == ITYPE_R) {
        if (d->funct == FUNCT_MULT) {
            int64_t  res = (int64_t)(int32_t)d->rs_val *
                           (int64_t)(int32_t)d->rt_val;
            cpu->hi = (uint32_t)(res >> 32);
            cpu->lo = (uint32_t)(res & 0xFFFFFFFF);
        } else if (d->funct == FUNCT_MULTU) {
            uint64_t res = (uint64_t)d->rs_val * (uint64_t)d->rt_val;
            cpu->hi = (uint32_t)(res >> 32);
            cpu->lo = (uint32_t)(res & 0xFFFFFFFF);
        } else if (d->funct == FUNCT_DIV) {
            if (d->rt_val != 0) {
                cpu->lo = (uint32_t)((int32_t)d->rs_val / (int32_t)d->rt_val);
                cpu->hi = (uint32_t)((int32_t)d->rs_val % (int32_t)d->rt_val);
            }
        } else if (d->funct == FUNCT_DIVU) {
            if (d->rt_val != 0) {
                cpu->lo = d->rs_val / d->rt_val;
                cpu->hi = d->rs_val % d->rt_val;
            }
        } else if (d->funct == FUNCT_MTHI) {
            cpu->hi = d->rs_val;
        } else if (d->funct == FUNCT_MTLO) {
            cpu->lo = d->rs_val;
        }
    }

    /* ---- 목적 레지스터 결정 ---- */
    uint32_t dest_reg  = 0;
    uint32_t dest_val  = 0;
    int      do_write  = 0;

    if (d->link) {
        /* JAL: r31 = PC+8, JALR: rd = PC+8 */
        dest_reg = (d->jalr) ? d->rd : (uint32_t)REG_RA;
        dest_val = d->pc + 8;
        do_write = 1;
    } else if (d->reg_write) {
        if (d->type == ITYPE_R) {
            /* MFHI / MFLO */
            if (d->funct == FUNCT_MFHI) {
                dest_reg = d->rd; dest_val = cpu->hi; do_write = 1;
            } else if (d->funct == FUNCT_MFLO) {
                dest_reg = d->rd; dest_val = cpu->lo; do_write = 1;
            } else {
                dest_reg = d->rd;
                dest_val = alu_result;
                do_write = 1;
            }
        } else {
            /* I-type: rt 가 목적 레지스터 */
            dest_reg = d->rt;
            dest_val = d->mem_to_reg ? mem_data : alu_result;
            do_write = 1;
        }
    }

    /* $zero 쓰기 방지 */
    if (do_write && dest_reg != REG_ZERO) {
        cpu->reg[dest_reg] = dest_val;
    }

    /* ---- PC 업데이트 ---- */
    cpu->pc = next_pc;

    /* ---- Changed Architectural State 출력 ---- */
    if (verbose) {
        int changed = 0;
        /* PC 변화 */
        if (prev_pc != cpu->pc) {
            if (!changed) {
                printf("[Cycle PC=0x%08X] Changed state:\n", prev_pc);
                changed = 1;
            }
            printf("  PC: 0x%08X -> 0x%08X\n", prev_pc, cpu->pc);
        }
        /* 레지스터 변화 */
        for (int i = 1; i < NUM_REGS; i++) {
            if (prev_reg[i] != cpu->reg[i]) {
                if (!changed) {
                    printf("[Cycle PC=0x%08X] Changed state:\n", prev_pc);
                    changed = 1;
                }
                printf("  %s (r%d): 0x%08X -> 0x%08X\n",
                       reg_name[i], i, prev_reg[i], cpu->reg[i]);
            }
        }
        /* 메모리 변화 (Store 명령어 시) */
        if (d->mem_write) {
            uint32_t addr = alu_result;
            if (!changed) {
                printf("[Cycle PC=0x%08X] Changed state:\n", prev_pc);
                changed = 1;
            }
            switch ((Opcode)d->opcode) {
            case OP_SW:
                printf("  MEM[0x%08X]: 0x%08X\n", addr, d->rt_val);
                break;
            case OP_SH:
                printf("  MEM[0x%08X]: 0x%04X (half)\n", addr,
                       (uint16_t)(d->rt_val & 0xFFFF));
                break;
            case OP_SB:
                printf("  MEM[0x%08X]: 0x%02X (byte)\n", addr,
                       (uint8_t)(d->rt_val & 0xFF));
                break;
            default:
                break;
            }
        }
        if (!changed) {
            /* NOP 등 변화 없는 사이클은 출력 생략 */
        }
    }
}

/* ================================================================
 *  cpu_run: 메인 실행 루프
 * ================================================================ */
void cpu_run(CPUState *cpu, int verbose)
{
    ExecStats stats;
    memset(&stats, 0, sizeof(stats));

    while (cpu->pc != HALT_ADDR) {

        /* ---- 1. Instruction Fetch -------------------------------- */
        uint32_t raw = stage_if(cpu);

        /* ---- 2. Instruction Decode ------------------------------- */
        DecodedInstr d;
        stage_id(cpu, raw, cpu->pc, &d);

        /* ---- 통계 업데이트 (타입 계수) -------------------------- */
        stats.total++;
        switch (d.type) {
        case ITYPE_R: stats.r_type++; break;
        case ITYPE_I: stats.i_type++; break;
        case ITYPE_J: stats.j_type++; break;
        default: break;
        }
        if (d.is_branch)                     stats.branches++;
        /* mem_access 카운트는 stage_mem() 내부에서 증가 */

        /* ---- 3. Execution ---------------------------------------- */
        uint32_t next_pc;
        int      branch_taken;
        uint32_t alu_result = stage_ex(cpu, &d, &next_pc, &branch_taken);

        if (d.is_branch && branch_taken)      stats.taken++;
        /* JAL/JR 등 무조건 점프도 taken 으로 집계 */
        if (d.is_jump)                        stats.taken++;

        /* ---- 4. Memory ------------------------------------------- */
        uint32_t mem_data = stage_mem(cpu, &d, alu_result, &stats);

        /* ---- 5. Write Back --------------------------------------- */
        stage_wb(cpu, &d, alu_result, mem_data, next_pc, verbose);
    }

    /* 종료 통계 출력 */
    cpu_print_stats(&stats, cpu);
}

/* ================================================================
 *  cpu_print_stats: 종료 시 통계 출력
 * ================================================================ */
void cpu_print_stats(const ExecStats *stats, const CPUState *cpu)
{
    printf("\n========================================\n");
    printf("  Execution Complete\n");
    printf("========================================\n");
    printf("  Final return value (v0/r2) : 0x%08X (%u)\n",
           cpu->reg[REG_V0], cpu->reg[REG_V0]);
    printf("  Total instructions executed: %llu\n",
           (unsigned long long)stats->total);
    printf("  R-type instructions        : %llu\n",
           (unsigned long long)stats->r_type);
    printf("  I-type instructions        : %llu\n",
           (unsigned long long)stats->i_type);
    printf("  J-type instructions        : %llu\n",
           (unsigned long long)stats->j_type);
    printf("  Memory access instructions : %llu\n",
           (unsigned long long)stats->mem_access);
    printf("  Branch instructions (total): %llu\n",
           (unsigned long long)stats->branches);
    printf("  Effective taken branches   : %llu\n",
           (unsigned long long)stats->taken);
    printf("========================================\n");
}