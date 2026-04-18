#include "cpu.h"

void cpu_init(CPUState *cpu)
{
    memset(cpu->reg, 0, sizeof(cpu->reg));
    cpu->reg[REG_SP] = INIT_SP;
    cpu->reg[REG_RA] = INIT_LR;
    cpu->pc = INIT_PC;
    cpu->hi = 0;
    cpu->lo = 0;
}

static const char *reg_name[NUM_REGS] = {
    "$zero","$at","$v0","$v1","$a0","$a1","$a2","$a3",
    "$t0","$t1","$t2","$t3","$t4","$t5","$t6","$t7",
    "$s0","$s1","$s2","$s3","$s4","$s5","$s6","$s7",
    "$t8","$t9","$k0","$k1","$gp","$sp","$fp","$ra"
};

static uint32_t stage_if(const CPUState *cpu)
{
    return mem_read_word(cpu, cpu->pc);
}

static void stage_id(const CPUState *cpu, uint32_t raw, uint32_t pc,
                     DecodedInstr *d)
{
    decode_instruction(cpu, raw, pc, d);
}

static uint32_t stage_ex(const CPUState *cpu, DecodedInstr *d,
                         uint32_t *next_pc, int *branch_taken)
{
    uint32_t alu_result = 0;
    int      zero       = 0;

    uint32_t op_a  = d->rs_val;
    uint32_t op_b;
    uint32_t shamt = d->shamt;

    if (d->type == ITYPE_R) {
        op_b = d->rt_val;
    } else {
        op_b = (d->opcode == OP_ORI) ? (uint32_t)d->uimm : (uint32_t)d->imm;
    }

    ALUOp aop = alu_control(d);
    alu_execute(op_a, op_b, shamt, aop, &alu_result, &zero);

    uint32_t pc4 = d->pc + 4;
    *branch_taken = 0;
    *next_pc      = pc4;

    if (d->IsJump) {
        if (d->type == ITYPE_J)
            *next_pc = (pc4 & 0xF0000000u) | (d->target << 2);
        else
            *next_pc = d->rs_val;  /* JR */
        *branch_taken = 1;
    } else if (d->IsBranch) {
        int take = 0;
        switch ((Opcode)d->opcode) {
        case OP_BEQ: take = (d->rs_val == d->rt_val); break;
        case OP_BNE: take = (d->rs_val != d->rt_val); break;
        default:     take = 0;                         break;
        }
        if (take) {
            *next_pc      = (uint32_t)((int32_t)pc4 + (d->imm << 2));
            *branch_taken = 1;
        }
    }

    (void)cpu;
    return alu_result;
}

static uint32_t stage_mem(CPUState *cpu, const DecodedInstr *d,
                           uint32_t alu_result, ExecStats *stats)
{
    uint32_t mem_data = 0;

    if (d->MemRead) {
        stats->mem_access++;
        mem_data = mem_read_word(cpu, alu_result);
    } else if (d->MemWrite) {
        stats->mem_access++;
        mem_write_word(cpu, alu_result, d->rt_val);
    }

    return mem_data;
}

static void stage_wb(CPUState *cpu, const DecodedInstr *d,
                     uint32_t alu_result, uint32_t mem_data,
                     uint32_t next_pc)
{
    uint32_t prev_reg[NUM_REGS];
    memcpy(prev_reg, cpu->reg, sizeof(cpu->reg));
    uint32_t prev_pc = cpu->pc;

    uint32_t dest_reg = 0;
    uint32_t dest_val = 0;
    int      do_write = 0;

    if (d->Link) {
        dest_reg = REG_RA;
        dest_val = d->pc + 8;  /* JAL: r31 = PC+8 */
        do_write = 1;
    } else if (d->RegWrite) {
        if (d->type == ITYPE_R) {
            dest_reg = d->rd;
            dest_val = alu_result;
            do_write = 1;
        } else {
            dest_reg = d->rt;
            dest_val = d->MemtoReg ? mem_data : alu_result;
            do_write = 1;
        }
    }

    if (do_write && dest_reg != REG_ZERO)
        cpu->reg[dest_reg] = dest_val;

    cpu->pc = next_pc;

    int changed = 0;

    if (prev_pc != cpu->pc) {
        if (!changed) {
            printf("[Cycle PC=0x%08X] Changed state:\n", prev_pc);
            changed = 1;
        }
        printf("  PC: 0x%08X -> 0x%08X\n", prev_pc, cpu->pc);
    }

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

    if (d->MemWrite) {
        if (!changed) {
            printf("[Cycle PC=0x%08X] Changed state:\n", prev_pc);
            changed = 1;
        }
        printf("  MEM[0x%08X]: 0x%08X\n", alu_result, d->rt_val);
    }
}

void cpu_run(CPUState *cpu)
{
    ExecStats stats;
    memset(&stats, 0, sizeof(stats));

    while (cpu->pc != HALT_ADDR) {
        uint32_t raw = stage_if(cpu);

        DecodedInstr d;
        stage_id(cpu, raw, cpu->pc, &d);

        stats.total++;
        switch (d.type) {
        case ITYPE_R: stats.r_type++; break;
        case ITYPE_I: stats.i_type++; break;
        case ITYPE_J: stats.j_type++; break;
        default: break;
        }
        if (d.IsBranch) stats.branches++;

        uint32_t next_pc;
        int      branch_taken;
        uint32_t alu_result = stage_ex(cpu, &d, &next_pc, &branch_taken);

        if (d.IsBranch && branch_taken) stats.taken++;
        if (d.IsJump)                   stats.taken++;

        uint32_t mem_data = stage_mem(cpu, &d, alu_result, &stats);

        stage_wb(cpu, &d, alu_result, mem_data, next_pc);
    }

    cpu_print_stats(&stats, cpu);
}

void cpu_print_stats(const ExecStats *stats, const CPUState *cpu)
{
    printf("\n========================================\n");
    printf("  Execution Complete\n");
    printf("========================================\n");
    printf("  Final return value (v0/r2) : 0x%08X (%u)\n",
           cpu->reg[REG_V0], cpu->reg[REG_V0]);
    printf("  Total instructions executed: %llu\n", (unsigned long long)stats->total);
    printf("  R-type instructions        : %llu\n", (unsigned long long)stats->r_type);
    printf("  I-type instructions        : %llu\n", (unsigned long long)stats->i_type);
    printf("  J-type instructions        : %llu\n", (unsigned long long)stats->j_type);
    printf("  Memory access instructions : %llu\n", (unsigned long long)stats->mem_access);
    printf("  Branch instructions (total): %llu\n", (unsigned long long)stats->branches);
    printf("  Effective taken branches   : %llu\n", (unsigned long long)stats->taken);
    printf("========================================\n");
}
