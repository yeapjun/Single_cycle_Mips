#ifndef DECODE_H
#define DECODE_H

#include "mips.h"

/*
 * 32-bit 명령어 워드를 DecodedInstr 구조체로 파싱한다.
 * 제어 신호(reg_dst, alu_src, mem_read, …)도 이 단계에서 설정한다.
 * cpu 는 레지스터 파일 읽기(rs_val, rt_val)에만 사용한다.
 */
void decode_instruction(const CPUState *cpu, uint32_t raw,
                        uint32_t pc, DecodedInstr *d);

/* 디버그용: 디코딩 결과를 사람이 읽기 좋은 형태로 출력 */
void decode_print(const DecodedInstr *d);

#endif /* DECODE_H */