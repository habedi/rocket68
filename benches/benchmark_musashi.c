#include "bench_common.h"
#include "m68k.h"

uint8_t memory[0x10000];

unsigned int m68k_read_memory_8(unsigned int address) { return memory[address & 0xFFFF]; }

unsigned int m68k_read_memory_16(unsigned int address)
{
    return (memory[address & 0xFFFF] << 8) | memory[(address + 1) & 0xFFFF];
}

unsigned int m68k_read_memory_32(unsigned int address)
{
    return (m68k_read_memory_16(address) << 16) | m68k_read_memory_16(address + 2);
}

unsigned int m68k_read_disassembler_8(unsigned int address) { return m68k_read_memory_8(address); }
unsigned int m68k_read_disassembler_16(unsigned int address) { return m68k_read_memory_16(address); }
unsigned int m68k_read_disassembler_32(unsigned int address) { return m68k_read_memory_32(address); }

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    memory[address & 0xFFFF] = value & 0xFF;
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    memory[address & 0xFFFF] = (value >> 8) & 0xFF;
    memory[(address + 1) & 0xFFFF] = value & 0xFF;
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    m68k_write_memory_16(address, value >> 16);
    m68k_write_memory_16(address + 2, value & 0xFFFF);
}

static int run_bench(const BenchWorkload* w)
{
    memset(memory, 0, sizeof(memory));
    load_program(memory, w->code, w->code_words);

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    m68k_set_reg(M68K_REG_SP, 0xFF00);
    m68k_set_reg(M68K_REG_A0, 0x8000);
    m68k_set_reg(M68K_REG_SR, 0x2700);
    m68k_set_reg(M68K_REG_PC, 0);
    const unsigned int stop_end_pc = find_stop_end_pc(w);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t slices = 0;
    for (;;)
    {
        m68k_execute(BENCH_SLICE_CYCLES);
        slices++;
        unsigned int pc = m68k_get_reg(NULL, M68K_REG_PC);
        unsigned int d0 = m68k_get_reg(NULL, M68K_REG_D0);
        /* STOP may be in the middle of the code buffer (e.g. call workload). */
        if (pc >= stop_end_pc && d0 == 0) break;
        if ((slices & 0xFFu) == 0 &&
            bench_guard_exceeded("Musashi", w->name, slices, &start))
        {
            return 1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    print_result(w->name, time_diff(&start, &end), w->expected_iterations);
    return 0;
}

int main(void)
{
    printf("Musashi Benchmark\n");
    printf("=================\n");

    BenchWorkload workloads[] = BENCH_WORKLOADS;
    size_t n = sizeof(workloads) / sizeof(workloads[0]);

    for (size_t i = 0; i < n; i++)
    {
        if (run_bench(&workloads[i]) != 0)
        {
            return 1;
        }
    }

    return 0;
}
