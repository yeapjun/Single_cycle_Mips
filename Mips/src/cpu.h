#ifndef CPU_H
#define CPU_H

#include "mips.h"
#include "memory.h"
#include "decode.h"
#include "alu.h"

void cpu_init(CPUState *cpu);
void cpu_run(CPUState *cpu);
void cpu_print_stats(const ExecStats *stats, const CPUState *cpu);

#endif /* CPU_H */
