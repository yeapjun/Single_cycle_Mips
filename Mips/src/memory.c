#include "memory.h"

void mem_init(CPUState *cpu)
{
    cpu->mem_size = MEM_SIZE;
    cpu->mem = (uint8_t *)calloc(MEM_SIZE, sizeof(uint8_t));
    if (!cpu->mem) {
        fprintf(stderr, "[ERROR] Failed to allocate %u bytes for memory.\n", MEM_SIZE);
        exit(EXIT_FAILURE);
    }
}

void mem_free(CPUState *cpu)
{
    free(cpu->mem);
    cpu->mem = NULL;
}

int mem_load_binary(CPUState *cpu, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open binary file: %s\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    if (fsize <= 0) {
        fprintf(stderr, "[ERROR] Binary file is empty or unreadable.\n");
        fclose(fp);
        return -1;
    }
    if ((uint32_t)fsize > cpu->mem_size - LOAD_ADDR) {
        fprintf(stderr, "[ERROR] Binary file too large: %ld bytes.\n", fsize);
        fclose(fp);
        return -1;
    }

    size_t read_bytes = fread(cpu->mem + LOAD_ADDR, 1, (size_t)fsize, fp);
    fclose(fp);

    if ((long)read_bytes != fsize) {
        fprintf(stderr, "[ERROR] Partial read: %zu / %ld bytes.\n", read_bytes, fsize);
        return -1;
    }

    printf("[INFO] Loaded %ld bytes from \"%s\" at 0x%08X.\n", fsize, filename, LOAD_ADDR);
    return 0;
}

static void check_addr(const CPUState *cpu, uint32_t addr, uint32_t width, const char *op)
{
    if (addr + width > cpu->mem_size) {
        fprintf(stderr, "[ERROR] Memory %s out of bounds: addr=0x%08X width=%u (mem_size=0x%08X)\n",
                op, addr, width, cpu->mem_size);
        exit(EXIT_FAILURE);
    }
}

uint8_t mem_read_byte(const CPUState *cpu, uint32_t addr)
{
    check_addr(cpu, addr, 1, "read");
    return cpu->mem[addr];
}

uint16_t mem_read_half(const CPUState *cpu, uint32_t addr)
{
    check_addr(cpu, addr, 2, "read");
    return (uint16_t)((cpu->mem[addr] << 8) | cpu->mem[addr + 1]);
}

uint32_t mem_read_word(const CPUState *cpu, uint32_t addr)
{
    check_addr(cpu, addr, 4, "read");
    return ((uint32_t)cpu->mem[addr]     << 24) |
           ((uint32_t)cpu->mem[addr + 1] << 16) |
           ((uint32_t)cpu->mem[addr + 2] <<  8) |
            (uint32_t)cpu->mem[addr + 3];
}

void mem_write_byte(CPUState *cpu, uint32_t addr, uint8_t val)
{
    check_addr(cpu, addr, 1, "write");
    cpu->mem[addr] = val;
}

void mem_write_half(CPUState *cpu, uint32_t addr, uint16_t val)
{
    check_addr(cpu, addr, 2, "write");
    cpu->mem[addr]     = (uint8_t)(val >> 8);
    cpu->mem[addr + 1] = (uint8_t)(val & 0xFF);
}

void mem_write_word(CPUState *cpu, uint32_t addr, uint32_t val)
{
    check_addr(cpu, addr, 4, "write");
    cpu->mem[addr]     = (uint8_t)((val >> 24) & 0xFF);
    cpu->mem[addr + 1] = (uint8_t)((val >> 16) & 0xFF);
    cpu->mem[addr + 2] = (uint8_t)((val >>  8) & 0xFF);
    cpu->mem[addr + 3] = (uint8_t)( val        & 0xFF);
}
