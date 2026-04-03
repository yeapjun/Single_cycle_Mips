/*
 * Mobile Processor Programming Assignment #2
 * Single-cycle MIPS Simulator
 *
 * Supports:
 *   - R-type, I-type, J-type instructions (MIPS 31 integer instructions)
 *   - Optional: JALR (funct=0x9, rd=31, rs=target)
 *
 * Initialization:
 *   - All registers = 0, except r29(SP)=0x10000000, r31(LR)=0xFFFFFFFF
 *   - Binary loaded at address 0x0
 *   - Execution halts when PC == 0xFFFFFFFF
 *
 * Output per cycle:
 *   - Changed architectural state (registers, PC, memory)
 * Output at end:
 *   - Final return value (r2)
 *   - Instruction counts (total, R, I, J, memory, branch, taken branch)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */
#define MEM_SIZE        (1 << 25)   /* 32 MB memory */
#define NUM_REGS        32
#define HALT_ADDR       0xFFFFFFFFu
#define INIT_SP         0x10000000u
#define INIT_LR         0xFFFFFFFFu

/* R-type funct codes */
#define FUNCT_SLL   0x00
#define FUNCT_SRL   0x02
#define FUNCT_SRA   0x03
#define FUNCT_SLLV  0x04
#define FUNCT_SRLV  0x06
#define FUNCT_SRAV  0x07
#define FUNCT_JR    0x08
#define FUNCT_JALR  0x09
#define FUNCT_MFHI  0x10
#define FUNCT_MTHI  0x11
#define FUNCT_MFLO  0x12
#define FUNCT_MTLO  0x13
#define FUNCT_MULT  0x18
#define FUNCT_MULTU 0x19
#define FUNCT_DIV   0x1A
#define FUNCT_DIVU  0x1B
#define FUNCT_ADD   0x20
#define FUNCT_ADDU  0x21
#define FUNCT_SUB   0x22
#define FUNCT_SUBU  0x23
#define FUNCT_AND   0x24
#define FUNCT_OR    0x25
#define FUNCT_XOR   0x26
#define FUNCT_NOR   0x27
#define FUNCT_SLT   0x2A
#define FUNCT_SLTU  0x2B

/* Opcodes */
#define OP_RTYPE    0x00
#define OP_J        0x02
#define OP_JAL      0x03
#define OP_BEQ      0x04
#define OP_BNE      0x05
#define OP_BLEZ     0x06
#define OP_BGTZ     0x07
#define OP_ADDI     0x08
#define OP_ADDIU    0x09
#define OP_SLTI     0x0A
#define OP_SLTIU    0x0B
#define OP_ANDI     0x0C
#define OP_ORI      0x0D
#define OP_XORI     0x0E
#define OP_LUI      0x0F
#define OP_LB       0x20
#define OP_LH       0x21
#define OP_LW       0x23
#define OP_LBU      0x24
#define OP_LHU      0x25
#define OP_SB       0x28
#define OP_SH       0x29
#define OP_SW       0x2B
#define OP_BLTZ_BGEZ 0x01  /* rt=0: BLTZ, rt=1: BGEZ */

/* ------------------------------------------------------------------ */
/*  CPU State                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t reg[NUM_REGS];   /* r0 ~ r31 */
    uint32_t pc;
    uint32_t hi, lo;          /* HI/LO for mult/div */
    uint8_t  mem[MEM_SIZE];
} CPU;

/* ------------------------------------------------------------------ */
/*  Statistics                                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    uint64_t total;
    uint64_t r_type;
    uint64_t i_type;
    uint64_t j_type;
    uint64_t mem_access;    /* load + store */
    uint64_t branch;        /* all branch instr */
    uint64_t taken_branch;  /* actually taken */
} Stats;

/* ------------------------------------------------------------------ */
/*  Helper: memory read / write (big-endian MIPS)                     */
/* ------------------------------------------------------------------ */
static uint32_t mem_read32(CPU *cpu, uint32_t addr) {
    if (addr + 3 >= MEM_SIZE) {
        fprintf(stderr, "[ERROR] Memory read out of bounds: 0x%08X\n", addr);
        exit(1);
    }
    return ((uint32_t)cpu->mem[addr]     << 24) |
           ((uint32_t)cpu->mem[addr + 1] << 16) |
           ((uint32_t)cpu->mem[addr + 2] <<  8) |
           ((uint32_t)cpu->mem[addr + 3]);
}

static uint16_t mem_read16(CPU *cpu, uint32_t addr) {
    if (addr + 1 >= MEM_SIZE) {
        fprintf(stderr, "[ERROR] Memory read16 out of bounds: 0x%08X\n", addr);
        exit(1);
    }
    return ((uint16_t)cpu->mem[addr] << 8) | cpu->mem[addr + 1];
}

static uint8_t mem_read8(CPU *cpu, uint32_t addr) {
    if (addr >= MEM_SIZE) {
        fprintf(stderr, "[ERROR] Memory read8 out of bounds: 0x%08X\n", addr);
        exit(1);
    }
    return cpu->mem[addr];
}

static void mem_write32(CPU *cpu, uint32_t addr, uint32_t val) {
    if (addr + 3 >= MEM_SIZE) {
        fprintf(stderr, "[ERROR] Memory write out of bounds: 0x%08X\n", addr);
        exit(1);
    }
    cpu->mem[addr]     = (val >> 24) & 0xFF;
    cpu->mem[addr + 1] = (val >> 16) & 0xFF;
    cpu->mem[addr + 2] = (val >>  8) & 0xFF;
    cpu->mem[addr + 3] = (val      ) & 0xFF;
}

static void mem_write16(CPU *cpu, uint32_t addr, uint16_t val) {
    if (addr + 1 >= MEM_SIZE) {
        fprintf(stderr, "[ERROR] Memory write16 out of bounds: 0x%08X\n", addr);
        exit(1);
    }
    cpu->mem[addr]     = (val >> 8) & 0xFF;
    cpu->mem[addr + 1] = (val     ) & 0xFF;
}

static void mem_write8(CPU *cpu, uint32_t addr, uint8_t val) {
    if (addr >= MEM_SIZE) {
        fprintf(stderr, "[ERROR] Memory write8 out of bounds: 0x%08X\n", addr);
        exit(1);
    }
    cpu->mem[addr] = val;
}

/* ------------------------------------------------------------------ */
/*  Register names                                                     */
/* ------------------------------------------------------------------ */
static const char *reg_name[NUM_REGS] = {
    "zero","at","v0","v1","a0","a1","a2","a3",
    "t0","t1","t2","t3","t4","t5","t6","t7",
    "s0","s1","s2","s3","s4","s5","s6","s7",
    "t8","t9","k0","k1","gp","sp","fp","ra"
};

/* ------------------------------------------------------------------ */
/*  Stage 1: Instruction Fetch                                         */
/* ------------------------------------------------------------------ */
static uint32_t stage_fetch(CPU *cpu) {
    return mem_read32(cpu, cpu->pc);
}

/* ------------------------------------------------------------------ */
/*  Decoded instruction fields                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t  opcode;
    uint8_t  rs, rt, rd, shamt, funct;
    uint16_t imm16;
    uint32_t target26;
    int32_t  simm;   /* sign-extended immediate */
} Decoded;

/* ------------------------------------------------------------------ */
/*  Stage 2: Instruction Decode                                        */
/* ------------------------------------------------------------------ */
static Decoded stage_decode(uint32_t instr) {
    Decoded d;
    d.opcode   = (instr >> 26) & 0x3F;
    d.rs       = (instr >> 21) & 0x1F;
    d.rt       = (instr >> 16) & 0x1F;
    d.rd       = (instr >> 11) & 0x1F;
    d.shamt    = (instr >>  6) & 0x1F;
    d.funct    = (instr      ) & 0x3F;
    d.imm16    = (instr      ) & 0xFFFF;
    d.target26 = (instr      ) & 0x3FFFFFF;
    /* sign-extend immediate */
    d.simm = (int32_t)(int16_t)d.imm16;
    return d;
}

/* ------------------------------------------------------------------ */
/*  Stage 3: Execute  +  Stage 4: Memory  +  Stage 5: Writeback       */
/*  (single-cycle: all in one function, state tracked for printing)   */
/* ------------------------------------------------------------------ */

/* We track changed state for output */
typedef struct {
    /* registers changed */
    int     reg_changed[NUM_REGS];
    uint32_t reg_old[NUM_REGS];
    /* PC */
    uint32_t pc_old, pc_new;
    /* memory changed (up to 1 location per instruction) */
    int      mem_changed;
    uint32_t mem_addr;
    uint32_t mem_old_val, mem_new_val;
    uint8_t  mem_size;  /* 1,2,4 bytes */
} ChangeLog;

static void execute_instruction(CPU *cpu, uint32_t instr, Stats *stats, ChangeLog *log) {
    Decoded d = stage_decode(instr);

    /* Save snapshot for logging */
    memset(log, 0, sizeof(*log));
    log->pc_old = cpu->pc;
    for (int i = 0; i < NUM_REGS; i++) log->reg_old[i] = cpu->reg[i];

    uint32_t next_pc = cpu->pc + 4;
    uint32_t alu_result = 0;
    int      branch_taken = 0;

    /* r0 is always zero; we enforce at writeback */

    if (d.opcode == OP_RTYPE) {
        /* ---- R-type ---- */
        stats->r_type++;
        stats->total++;

        uint32_t rs_val = cpu->reg[d.rs];
        uint32_t rt_val = cpu->reg[d.rt];

        switch (d.funct) {
            case FUNCT_SLL:
                alu_result = rt_val << d.shamt;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SRL:
                alu_result = rt_val >> d.shamt;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SRA:
                alu_result = (uint32_t)((int32_t)rt_val >> d.shamt);
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SLLV:
                alu_result = rt_val << (rs_val & 0x1F);
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SRLV:
                alu_result = rt_val >> (rs_val & 0x1F);
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SRAV:
                alu_result = (uint32_t)((int32_t)rt_val >> (rs_val & 0x1F));
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_JR:
                next_pc = rs_val;
                break;
            case FUNCT_JALR: /* Optional: JALR */
                cpu->reg[d.rd] = cpu->pc + 4;
                next_pc = rs_val;
                break;
            case FUNCT_MFHI:
                cpu->reg[d.rd] = cpu->hi;
                break;
            case FUNCT_MTHI:
                cpu->hi = rs_val;
                break;
            case FUNCT_MFLO:
                cpu->reg[d.rd] = cpu->lo;
                break;
            case FUNCT_MTLO:
                cpu->lo = rs_val;
                break;
            case FUNCT_MULT: {
                int64_t result = (int64_t)(int32_t)rs_val * (int64_t)(int32_t)rt_val;
                cpu->lo = (uint32_t)(result & 0xFFFFFFFF);
                cpu->hi = (uint32_t)(result >> 32);
                break;
            }
            case FUNCT_MULTU: {
                uint64_t result = (uint64_t)rs_val * (uint64_t)rt_val;
                cpu->lo = (uint32_t)(result & 0xFFFFFFFF);
                cpu->hi = (uint32_t)(result >> 32);
                break;
            }
            case FUNCT_DIV:
                if (rt_val != 0) {
                    cpu->lo = (uint32_t)((int32_t)rs_val / (int32_t)rt_val);
                    cpu->hi = (uint32_t)((int32_t)rs_val % (int32_t)rt_val);
                }
                break;
            case FUNCT_DIVU:
                if (rt_val != 0) {
                    cpu->lo = rs_val / rt_val;
                    cpu->hi = rs_val % rt_val;
                }
                break;
            case FUNCT_ADD:
            case FUNCT_ADDU:
                alu_result = rs_val + rt_val;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SUB:
            case FUNCT_SUBU:
                alu_result = rs_val - rt_val;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_AND:
                alu_result = rs_val & rt_val;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_OR:
                alu_result = rs_val | rt_val;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_XOR:
                alu_result = rs_val ^ rt_val;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_NOR:
                alu_result = ~(rs_val | rt_val);
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SLT:
                alu_result = ((int32_t)rs_val < (int32_t)rt_val) ? 1 : 0;
                cpu->reg[d.rd] = alu_result;
                break;
            case FUNCT_SLTU:
                alu_result = (rs_val < rt_val) ? 1 : 0;
                cpu->reg[d.rd] = alu_result;
                break;
            default:
                fprintf(stderr, "[WARN] Unknown R-type funct=0x%02X at PC=0x%08X\n",
                        d.funct, cpu->pc);
                break;
        }

    } else if (d.opcode == OP_J) {
        /* ---- J-type: J ---- */
        stats->j_type++;
        stats->total++;
        stats->branch++;
        stats->taken_branch++;
        next_pc = ((cpu->pc + 4) & 0xF0000000) | (d.target26 << 2);
        branch_taken = 1;

    } else if (d.opcode == OP_JAL) {
        /* ---- J-type: JAL ---- */
        stats->j_type++;
        stats->total++;
        stats->branch++;
        stats->taken_branch++;
        cpu->reg[31] = cpu->pc + 4;
        next_pc = ((cpu->pc + 4) & 0xF0000000) | (d.target26 << 2);
        branch_taken = 1;

    } else {
        /* ---- I-type ---- */
        stats->i_type++;
        stats->total++;

        uint32_t rs_val = cpu->reg[d.rs];
        uint32_t rt_val = cpu->reg[d.rt];
        uint32_t mem_addr;

        switch (d.opcode) {
            /* Branches */
            case OP_BEQ:
                stats->branch++;
                if (rs_val == rt_val) {
                    next_pc = (cpu->pc + 4) + (d.simm << 2);
                    branch_taken = 1;
                    stats->taken_branch++;
                }
                break;
            case OP_BNE:
                stats->branch++;
                if (rs_val != rt_val) {
                    next_pc = (cpu->pc + 4) + (d.simm << 2);
                    branch_taken = 1;
                    stats->taken_branch++;
                }
                break;
            case OP_BLEZ:
                stats->branch++;
                if ((int32_t)rs_val <= 0) {
                    next_pc = (cpu->pc + 4) + (d.simm << 2);
                    branch_taken = 1;
                    stats->taken_branch++;
                }
                break;
            case OP_BGTZ:
                stats->branch++;
                if ((int32_t)rs_val > 0) {
                    next_pc = (cpu->pc + 4) + (d.simm << 2);
                    branch_taken = 1;
                    stats->taken_branch++;
                }
                break;
            case OP_BLTZ_BGEZ:
                stats->branch++;
                if (d.rt == 0) {   /* BLTZ */
                    if ((int32_t)rs_val < 0) {
                        next_pc = (cpu->pc + 4) + (d.simm << 2);
                        branch_taken = 1;
                        stats->taken_branch++;
                    }
                } else if (d.rt == 1) { /* BGEZ */
                    if ((int32_t)rs_val >= 0) {
                        next_pc = (cpu->pc + 4) + (d.simm << 2);
                        branch_taken = 1;
                        stats->taken_branch++;
                    }
                }
                break;

            /* ALU immediate */
            case OP_ADDI:
            case OP_ADDIU:
                cpu->reg[d.rt] = rs_val + (uint32_t)d.simm;
                break;
            case OP_SLTI:
                cpu->reg[d.rt] = ((int32_t)rs_val < d.simm) ? 1 : 0;
                break;
            case OP_SLTIU:
                cpu->reg[d.rt] = (rs_val < (uint32_t)(int32_t)d.simm) ? 1 : 0;
                break;
            case OP_ANDI:
                cpu->reg[d.rt] = rs_val & (uint32_t)d.imm16;  /* zero-extend */
                break;
            case OP_ORI:
                cpu->reg[d.rt] = rs_val | (uint32_t)d.imm16;
                break;
            case OP_XORI:
                cpu->reg[d.rt] = rs_val ^ (uint32_t)d.imm16;
                break;
            case OP_LUI:
                cpu->reg[d.rt] = (uint32_t)d.imm16 << 16;
                break;

            /* Memory loads */
            case OP_LB:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                cpu->reg[d.rt] = (uint32_t)(int32_t)(int8_t)mem_read8(cpu, mem_addr);
                break;
            case OP_LBU:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                cpu->reg[d.rt] = (uint32_t)mem_read8(cpu, mem_addr);
                break;
            case OP_LH:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                cpu->reg[d.rt] = (uint32_t)(int32_t)(int16_t)mem_read16(cpu, mem_addr);
                break;
            case OP_LHU:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                cpu->reg[d.rt] = (uint32_t)mem_read16(cpu, mem_addr);
                break;
            case OP_LW:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                cpu->reg[d.rt] = mem_read32(cpu, mem_addr);
                break;

            /* Memory stores */
            case OP_SB:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                log->mem_changed = 1;
                log->mem_addr    = mem_addr;
                log->mem_old_val = mem_read8(cpu, mem_addr);
                log->mem_size    = 1;
                mem_write8(cpu, mem_addr, (uint8_t)(rt_val & 0xFF));
                log->mem_new_val = mem_read8(cpu, mem_addr);
                break;
            case OP_SH:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                log->mem_changed = 1;
                log->mem_addr    = mem_addr;
                log->mem_old_val = mem_read16(cpu, mem_addr);
                log->mem_size    = 2;
                mem_write16(cpu, mem_addr, (uint16_t)(rt_val & 0xFFFF));
                log->mem_new_val = mem_read16(cpu, mem_addr);
                break;
            case OP_SW:
                stats->mem_access++;
                mem_addr = rs_val + (uint32_t)d.simm;
                log->mem_changed = 1;
                log->mem_addr    = mem_addr;
                log->mem_old_val = mem_read32(cpu, mem_addr);
                log->mem_size    = 4;
                mem_write32(cpu, mem_addr, rt_val);
                log->mem_new_val = mem_read32(cpu, mem_addr);
                break;

            default:
                fprintf(stderr, "[WARN] Unknown opcode=0x%02X at PC=0x%08X\n",
                        d.opcode, cpu->pc);
                break;
        }
        (void)branch_taken;
    }

    /* Enforce r0 = 0 always */
    cpu->reg[0] = 0;

    /* Update PC */
    cpu->pc = next_pc;
    log->pc_new = cpu->pc;

    /* Detect register changes */
    for (int i = 0; i < NUM_REGS; i++) {
        if (cpu->reg[i] != log->reg_old[i]) {
            log->reg_changed[i] = 1;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Print changed state after each cycle                              */
/* ------------------------------------------------------------------ */
static void print_cycle_state(uint32_t instr, const ChangeLog *log) {
    Decoded d = stage_decode(instr);

    /* Print instruction opcode info */
    if (d.opcode == OP_RTYPE) {
        printf("[PC=0x%08X] R-type  funct=0x%02X  | instr=0x%08X\n",
               log->pc_old, d.funct, instr);
    } else if (d.opcode == OP_J || d.opcode == OP_JAL) {
        printf("[PC=0x%08X] J-type  opcode=0x%02X | instr=0x%08X\n",
               log->pc_old, d.opcode, instr);
    } else {
        printf("[PC=0x%08X] I-type  opcode=0x%02X | instr=0x%08X\n",
               log->pc_old, d.opcode, instr);
    }

    /* Changed registers */
    int any_change = 0;
    for (int i = 0; i < NUM_REGS; i++) {
        if (log->reg_changed[i]) {
            printf("  r%02d($%s): 0x%08X -> 0x%08X\n",
                   i, reg_name[i], log->reg_old[i],
                   /* We need current value: old + change */ log->reg_old[i]);
            /* We'll print new value: we don't have direct ref to cpu here,
               so compute from old via diff is non-trivial.
               Instead, the caller provides current cpu state. */
            any_change = 1;
        }
    }
    (void)any_change;

    /* PC change */
    if (log->pc_new != log->pc_old + 4) {
        printf("  PC: 0x%08X -> 0x%08X\n", log->pc_old, log->pc_new);
    }

    /* Memory change */
    if (log->mem_changed) {
        printf("  MEM[0x%08X](%dB): 0x%08X -> 0x%08X\n",
               log->mem_addr, log->mem_size, log->mem_old_val, log->mem_new_val);
    }
}

/* Revised print that also takes cpu pointer for new reg values */
static void print_cycle_state_v2(CPU *cpu, uint32_t instr, const ChangeLog *log) {
    Decoded d = stage_decode(instr);

    if (d.opcode == OP_RTYPE) {
        printf("[PC=0x%08X] R-type  funct=0x%02X  | instr=0x%08X\n",
               log->pc_old, d.funct, instr);
    } else if (d.opcode == OP_J || d.opcode == OP_JAL) {
        printf("[PC=0x%08X] J-type  opcode=0x%02X | instr=0x%08X\n",
               log->pc_old, d.opcode, instr);
    } else {
        printf("[PC=0x%08X] I-type  opcode=0x%02X | instr=0x%08X\n",
               log->pc_old, d.opcode, instr);
    }

    /* Changed registers */
    for (int i = 0; i < NUM_REGS; i++) {
        if (log->reg_changed[i]) {
            printf("  r%02d($%-4s): 0x%08X -> 0x%08X\n",
                   i, reg_name[i], log->reg_old[i], cpu->reg[i]);
        }
    }

    /* PC change (always print PC) */
    printf("  PC:  0x%08X -> 0x%08X\n", log->pc_old, log->pc_new);

    /* Memory change */
    if (log->mem_changed) {
        printf("  MEM[0x%08X](%dB): 0x%08X -> 0x%08X\n",
               log->mem_addr, log->mem_size, log->mem_old_val, log->mem_new_val);
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        fprintf(stderr, "  e.g., %s simple.bin\n", argv[0]);
        return 1;
    }

    /* Allocate CPU on heap (large memory array) */
    CPU *cpu = (CPU *)calloc(1, sizeof(CPU));
    if (!cpu) {
        fprintf(stderr, "[ERROR] Failed to allocate CPU state.\n");
        return 1;
    }

    /* ----- Initialize CPU state ----- */
    memset(cpu->reg, 0, sizeof(cpu->reg));
    cpu->pc     = 0x00000000;
    cpu->hi     = 0;
    cpu->lo     = 0;
    cpu->reg[29] = INIT_SP;   /* SP */
    cpu->reg[31] = INIT_LR;   /* LR */

    /* ----- Load binary into memory at 0x0 ----- */
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open binary file: %s\n", argv[1]);
        free(cpu);
        return 1;
    }
    size_t bytes_read = fread(cpu->mem, 1, MEM_SIZE, fp);
    fclose(fp);
    printf("=== MIPS Simulator Loaded: %zu bytes from '%s' ===\n\n", bytes_read, argv[1]);

    /* ----- Statistics ----- */
    Stats stats;
    memset(&stats, 0, sizeof(stats));

    /* ----- Execution Loop ----- */
    printf("=== Execution Trace ===\n\n");

    while (cpu->pc != HALT_ADDR) {
        /* Safety check: avoid infinite loops in bad binaries */
        if (cpu->pc >= MEM_SIZE) {
            fprintf(stderr, "[ERROR] PC out of memory range: 0x%08X\n", cpu->pc);
            break;
        }

        /* Stage 1: Fetch */
        uint32_t instr = stage_fetch(cpu);

        /* NOP (all zeros) at end of text section: treat as halt guard */
        /* Actually only halt on HALT_ADDR; skip printing nops that are
           genuinely at the beginning if present */

        /* Stage 2-5: Decode + Execute + Memory + Writeback */
        ChangeLog log;
        execute_instruction(cpu, instr, &stats, &log);

        /* Output: changed architectural state */
        print_cycle_state_v2(cpu, instr, &log);

        /* Prevent runaway (safety ceiling) */
        if (stats.total > 10000000ULL) {
            fprintf(stderr, "[WARN] Exceeded 10M instructions — possible infinite loop. Aborting.\n");
            break;
        }
    }

    /* ----- End of Execution Output ----- */
    printf("=== End of Execution ===\n");
    printf("  Final PC              : 0x%08X\n", cpu->pc);
    printf("  Return value (r2/v0)  : 0x%08X  (%d)\n", cpu->reg[2], (int32_t)cpu->reg[2]);
    printf("\n");
    printf("=== Instruction Statistics ===\n");
    printf("  Total instructions    : %llu\n", (unsigned long long)stats.total);
    printf("  R-type instructions   : %llu\n", (unsigned long long)stats.r_type);
    printf("  I-type instructions   : %llu\n", (unsigned long long)stats.i_type);
    printf("  J-type instructions   : %llu\n", (unsigned long long)stats.j_type);
    printf("  Memory accesses       : %llu\n", (unsigned long long)stats.mem_access);
    printf("  Branch instructions   : %llu\n", (unsigned long long)stats.branch);
    printf("  Taken branches        : %llu\n", (unsigned long long)stats.taken_branch);
    printf("\n");
    printf("=== Final Register State ===\n");
    for (int i = 0; i < NUM_REGS; i++) {
        if (cpu->reg[i] != 0) {
            printf("  r%02d($%-4s) = 0x%08X  (%d)\n",
                   i, reg_name[i], cpu->reg[i], (int32_t)cpu->reg[i]);
        }
    }

    free(cpu);
    return 0;
}