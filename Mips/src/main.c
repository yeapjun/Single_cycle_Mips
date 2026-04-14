#include "cpu.h"

/* ================================================================
 *  사용법 출력
 * ================================================================ */
static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-v] <binary_file>\n"
            "  -v   : verbose mode — print changed state every cycle\n"
            "  <binary_file> : MIPS binary (e.g. simple.bin)\n",
            prog);
}

/* ================================================================
 *  main
 * ================================================================ */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int         verbose  = 0;
    const char *binfile  = NULL;

    /* 간단한 인자 파싱 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else {
            binfile = argv[i];
        }
    }

    if (!binfile) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

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

    printf("[INFO] Starting MIPS simulation (verbose=%s)...\n\n",
           verbose ? "ON" : "OFF");

    /* ---- 4. 실행 루프 ---- */
    cpu_run(&cpu, verbose);

    /* ---- 5. 메모리 해제 ---- */
    mem_free(&cpu);
    return EXIT_SUCCESS;
}