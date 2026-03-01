#include "bench_common.h"
#include "m68k.h"

static int run_bench(const BenchWorkload* w)
{
    uint8_t memory[0x10000];
    M68kCpu cpu;
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    load_program(memory, w->code, w->code_words);

    cpu.a_regs[7].l = 0xFF00;
    cpu.a_regs[0].l = 0x8000;
    cpu.ssp = 0xFF00;
    cpu.sr = 0x2700;
    cpu.pc = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t slices = 0;
    while (!cpu.stopped)
    {
        m68k_execute(&cpu, BENCH_SLICE_CYCLES);
        slices++;
        if ((slices & 0xFFu) == 0 &&
            bench_guard_exceeded("Rocket68", w->name, slices, &start))
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
    printf("Rocket68 Benchmark\n");
    printf("==================\n");

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
