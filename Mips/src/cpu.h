#ifndef CPU_H
#define CPU_H

#include "mips.h"
#include "memory.h"
#include "decode.h"
#include "alu.h"

/* CPU 상태 초기화 */
void cpu_init(CPUState *cpu);

/* 메인 실행 루프 — PC 가 HALT_ADDR 이 될 때까지 반복 */
void cpu_run(CPUState *cpu, int verbose);

/* 종료 통계 출력 */
void cpu_print_stats(const ExecStats *stats, const CPUState *cpu);

#endif /* CPU_H */