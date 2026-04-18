#ifndef DECODE_H
#define DECODE_H

#include "mips.h"

void decode_instruction(const CPUState *cpu, uint32_t raw,
                        uint32_t pc, DecodedInstr *d);

#endif /* DECODE_H */
