#include "cpu.h"

/* ================================================================
 *  main
 * ================================================================ */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *binfile = argv[1];

    /* ---- 1. 메모리 할당 ---- */
    CPUState cpu;
    mem_init(&cpu);

    /* ---- 2. 레지스터 / PC 초기화 ---- */
    cpu_init(&cpu);

    /* ---- 3. 바이너리 파일 로드 ---- */
    if (mem_load_binary(&cpu, binfile) != 0) {
        mem_free(&cpu);
        return EXIT_FAILURE;
    }

    printf("[INFO] Starting MIPS simulation...\n\n");

    /* ---- 4. 실행 루프 (매 사이클 변경 상태 출력) ---- */
    cpu_run(&cpu);

    /* ---- 5. 메모리 해제 ---- */
    mem_free(&cpu);
    return EXIT_SUCCESS;
}