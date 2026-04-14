/*
 * Mobile Processor Programming Assignment #2
 * Single-cycle MIPS Simulator
 *
 * Supported instructions (derived from actual test programs):
 *
 *   R-type  : SLL (NOP), ADDU (move), SUBU, SLT, JR
 *   I-type  : ADDIU (li), SW, LW, BEQ (beqz), BNE (bne/bnez), SLTI, ORI
 *   J-type  : J, JAL
 *
 * Initialization:
 *   - All registers = 0, except r29(SP)=0x10000000, r31(RA)=0xFFFFFFFF
 *   - Binary loaded at address 0x0
 *   - Execution halts when PC == 0xFFFFFFFF
 *
 * Output per cycle:
 *   - Changed architectural state (registers, PC, memory)
 * Output at end:
 *   - Final return value (r2/v0)
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
#define FUNCT_JR    0x08
#define FUNCT_ADDU  0x21
#define FUNCT_SUBU  0x23
#define FUNCT_SLT   0x2A

/* Opcodes */
#define OP_RTYPE    0x00
#define OP_J        0x02
#define OP_JAL      0x03
#define OP_BEQ      0x04
#define OP_BNE      0x05
#define OP_ADDIU    0x09
#define OP_SLTI     0x0A
#define OP_ORI      0x0D
#define OP_LW       0x23
#define OP_SW       0x2B

/* ------------------------------------------------------------------ */
/*  CPU State                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t reg[NUM_REGS];
    uint32_t pc;
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
    uint64_t mem_access;
    uint64_t branch;
    uint64_t taken_branch;
} Stats;

/* ------------------------------------------------------------------ */
/*  Memory helpers (big-endian MIPS)                                  */
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
/*  Decoded instruction fields                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t  opcode;
    uint8_t  rs, rt, rd, shamt, funct;
    uint16_t imm16;
    uint32_t target26;
    int32_t  simm;
} Decoded;

static Decoded decode(uint32_t instr) {
    Decoded d;
    d.opcode   = (instr >> 26) & 0x3F;
    d.rs       = (instr >> 21) & 0x1F;
    d.rt       = (instr >> 16) & 0x1F;
    d.rd       = (instr >> 11) & 0x1F;
    d.shamt    = (instr >>  6) & 0x1F;
    d.funct    = (instr      ) & 0x3F;
    d.imm16    = (instr      ) & 0xFFFF;
    d.target26 = (instr      ) & 0x3FFFFFF;
    d.simm     = (int32_t)(int16_t)d.imm16;
    return d;
}

/* ------------------------------------------------------------------ */
/*  Change log for per-cycle output                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    int      reg_changed[NUM_REGS];
    uint32_t reg_old[NUM_REGS];
    uint32_t pc_old, pc_new;
    int      mem_changed;
    uint32_t mem_addr;
    uint32_t mem_old_val, mem_new_val;
} ChangeLog;

/* ------------------------------------------------------------------ */
/*  Execute                                                            */
/* ------------------------------------------------------------------ */
static void execute_instruction(CPU *cpu, uint32_t instr, Stats *stats, ChangeLog *log) {
    Decoded d = decode(instr);

    memset(log, 0, sizeof(*log));
    log->pc_old = cpu->pc;
    for (int i = 0; i < NUM_REGS; i++) log->reg_old[i] = cpu->reg[i];

    uint32_t next_pc = cpu->pc + 4;
    uint32_t rs = cpu->reg[d.rs];
    uint32_t rt = cpu->reg[d.rt];

    if (d.opcode == OP_RTYPE) {
        stats->r_type++;
        stats->total++;

        switch (d.funct) {
            case FUNCT_SLL:   /* also handles NOP (0x00000000) */
                cpu->reg[d.rd] = rt << d.shamt;
                break;
            case FUNCT_JR:
                next_pc = rs;
                break;
            case FUNCT_ADDU:  /* also encodes pseudo-op: move rd,rs */
                cpu->reg[d.rd] = rs + rt;
                break;
            case FUNCT_SUBU:
                cpu->reg[d.rd] = rs - rt;
                break;
            case FUNCT_SLT:
                cpu->reg[d.rd] = ((int32_t)rs < (int32_t)rt) ? 1 : 0;
                break;
            default:
                fprintf(stderr, "[WARN] Unimplemented R-type funct=0x%02X at PC=0x%08X\n",
                        d.funct, cpu->pc);
                break;
        }

    } else if (d.opcode == OP_J) {
        stats->j_type++;
        stats->total++;
        stats->branch++;
        stats->taken_branch++;
        next_pc = ((cpu->pc + 4) & 0xF0000000) | (d.target26 << 2);

    } else if (d.opcode == OP_JAL) {
        stats->j_type++;
        stats->total++;
        stats->branch++;
        stats->taken_branch++;
        cpu->reg[31] = cpu->pc + 4;
        next_pc = ((cpu->pc + 4) & 0xF0000000) | (d.target26 << 2);

    } else {
        stats->i_type++;
        stats->total++;

        switch (d.opcode) {
            case OP_BEQ:      /* also encodes: beqz rs,offset (rt=0) */
                stats->branch++;
                if (rs == rt) {
                    next_pc = (cpu->pc + 4) + (d.simm << 2);
                    stats->taken_branch++;
                }
                break;
            case OP_BNE:      /* also encodes: bnez rs,offset (rt=0) */
                stats->branch++;
                if (rs != rt) {
                    next_pc = (cpu->pc + 4) + (d.simm << 2);
                    stats->taken_branch++;
                }
                break;
            case OP_ADDIU:    /* also encodes: li rt,imm (rs=0) */
                cpu->reg[d.rt] = rs + (uint32_t)d.simm;
                break;
            case OP_SLTI:
                cpu->reg[d.rt] = ((int32_t)rs < d.simm) ? 1 : 0;
                break;
            case OP_ORI:      /* also encodes: li rt,imm (rs=0, imm<=0xFFFF) */
                cpu->reg[d.rt] = rs | (uint32_t)d.imm16;
                break;
            case OP_LW:
                stats->mem_access++;
                cpu->reg[d.rt] = mem_read32(cpu, rs + (uint32_t)d.simm);
                break;
            case OP_SW: {
                stats->mem_access++;
                uint32_t addr = rs + (uint32_t)d.simm;
                log->mem_changed = 1;
                log->mem_addr    = addr;
                log->mem_old_val = mem_read32(cpu, addr);
                mem_write32(cpu, addr, rt);
                log->mem_new_val = mem_read32(cpu, addr);
                break;
            }
            default:
                fprintf(stderr, "[WARN] Unimplemented opcode=0x%02X at PC=0x%08X\n",
                        d.opcode, cpu->pc);
                break;
        }
    }

    /* r0 is always 0 */
    cpu->reg[0] = 0;

    cpu->pc = next_pc;
    log->pc_new = cpu->pc;

    for (int i = 0; i < NUM_REGS; i++) {
        if (cpu->reg[i] != log->reg_old[i])
            log->reg_changed[i] = 1;
    }
}

/* ------------------------------------------------------------------ */
/*  Print changed state after each cycle                              */
/* ------------------------------------------------------------------ */
static void print_cycle(CPU *cpu, uint32_t instr, const ChangeLog *log) {
    Decoded d = decode(instr);

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

    for (int i = 0; i < NUM_REGS; i++) {
        if (log->reg_changed[i]) {
            printf("  r%02d($%-4s): 0x%08X -> 0x%08X\n",
                   i, reg_name[i], log->reg_old[i], cpu->reg[i]);
        }
    }

    printf("  PC:  0x%08X -> 0x%08X\n", log->pc_old, log->pc_new);

    if (log->mem_changed) {
        printf("  MEM[0x%08X](4B): 0x%08X -> 0x%08X\n",
               log->mem_addr, log->mem_old_val, log->mem_new_val);
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        fprintf(stderr, "  e.g., %s input.bin\n", argv[0]);
        return 1;
    }

    CPU *cpu = (CPU *)calloc(1, sizeof(CPU));
    if (!cpu) {
        fprintf(stderr, "[ERROR] Failed to allocate CPU state.\n");
        return 1;
    }

    /* Initialize CPU */
    cpu->pc      = 0x00000000;
    cpu->reg[29] = INIT_SP;
    cpu->reg[31] = INIT_LR;

    /* Load binary into memory at 0x0 */
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open binary file: %s\n", argv[1]);
        free(cpu);
        return 1;
    }
    size_t bytes_read = fread(cpu->mem, 1, MEM_SIZE, fp);
    fclose(fp);
    printf("=== MIPS Simulator: loaded %zu bytes from '%s' ===\n\n", bytes_read, argv[1]);

    Stats stats;
    memset(&stats, 0, sizeof(stats));

    printf("=== Execution Trace ===\n\n");

    while (cpu->pc != HALT_ADDR) {
        if (cpu->pc >= MEM_SIZE) {
            fprintf(stderr, "[ERROR] PC out of range: 0x%08X\n", cpu->pc);
            break;
        }

        uint32_t instr = mem_read32(cpu, cpu->pc);

        ChangeLog log;
        execute_instruction(cpu, instr, &stats, &log);
        print_cycle(cpu, instr, &log);

        if (stats.total > 10000000ULL) {
            fprintf(stderr, "[WARN] Exceeded 10M instructions — possible infinite loop. Aborting.\n");
            break;
        }
    }

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
