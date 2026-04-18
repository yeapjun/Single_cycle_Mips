#include "cpu.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    CPUState cpu;
    mem_init(&cpu);
    cpu_init(&cpu);

    if (mem_load_binary(&cpu, argv[1]) != 0) {
        mem_free(&cpu);
        return EXIT_FAILURE;
    }

    printf("[INFO] Starting MIPS simulation...\n\n");
    cpu_run(&cpu);

    mem_free(&cpu);
    return EXIT_SUCCESS;
}
