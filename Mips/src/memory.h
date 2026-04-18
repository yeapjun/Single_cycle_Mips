#ifndef MEMORY_H
#define MEMORY_H

#include "mips.h"

void     mem_init(CPUState *cpu);
void     mem_free(CPUState *cpu);
int      mem_load_binary(CPUState *cpu, const char *filename);

uint8_t  mem_read_byte(const CPUState *cpu, uint32_t addr);
uint16_t mem_read_half(const CPUState *cpu, uint32_t addr);
uint32_t mem_read_word(const CPUState *cpu, uint32_t addr);

void     mem_write_byte(CPUState *cpu, uint32_t addr, uint8_t  val);
void     mem_write_half(CPUState *cpu, uint32_t addr, uint16_t val);
void     mem_write_word(CPUState *cpu, uint32_t addr, uint32_t val);

#endif /* MEMORY_H */
