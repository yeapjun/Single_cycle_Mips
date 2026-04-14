#ifndef MEMORY_H
#define MEMORY_H

#include "mips.h"

/* 메모리 초기화 및 해제 */
void  mem_init(CPUState *cpu);
void  mem_free(CPUState *cpu);

/* 바이너리 파일을 메모리 LOAD_ADDR 위치에 로드 */
int   mem_load_binary(CPUState *cpu, const char *filename);

/* 바이트/하프워드/워드 읽기 (big-endian MIPS) */
uint8_t   mem_read_byte(const CPUState *cpu, uint32_t addr);
uint16_t  mem_read_half(const CPUState *cpu, uint32_t addr);
uint32_t  mem_read_word(const CPUState *cpu, uint32_t addr);

/* 바이트/하프워드/워드 쓰기 */
void  mem_write_byte(CPUState *cpu, uint32_t addr, uint8_t  val);
void  mem_write_half(CPUState *cpu, uint32_t addr, uint16_t val);
void  mem_write_word(CPUState *cpu, uint32_t addr, uint32_t val);

#endif /* MEMORY_H */