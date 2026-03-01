#include "bench_common.h"

#include <cstring>

#include "Moira.h"

namespace
{
    class BenchMoiraCPU : public moira::Moira
    {
    public:
        explicit BenchMoiraCPU(uint8_t* mem) : memory(mem)
        {
        }

    protected:
        moira::u8 read8(moira::u32 addr) const override
        {
            return memory[addr & 0xFFFFu];
        }

        moira::u16 read16(moira::u32 addr) const override
        {
            return (moira::u16)((memory[addr & 0xFFFFu] << 8) |
                memory[(addr + 1) & 0xFFFFu]);
        }

        moira::u16 read16OnReset(moira::u32 addr) const override
        {
            return read16(addr);
        }

        moira::u16 read16Dasm(moira::u32 addr) const override
        {
            return read16(addr);
        }

        void write8(moira::u32 addr, moira::u8 val) const override
        {
            memory[addr & 0xFFFFu] = val;
        }

        void write16(moira::u32 addr, moira::u16 val) const override
        {
            memory[addr & 0xFFFFu] = (uint8_t)((val >> 8) & 0xFFu);
            memory[(addr + 1) & 0xFFFFu] = (uint8_t)(val & 0xFFu);
        }

    private:
        uint8_t* memory;
    };

    static int run_bench(const BenchWorkload* w)
    {
        const uint32_t entry = 0x0100;
        uint8_t memory[0x10000];
        std::memset(memory, 0, sizeof(memory));

        /* Reset vectors: SSP at 0x00000000, PC at 0x00000004. */
        memory[0] = 0x00;
        memory[1] = 0x00;
        memory[2] = 0xFF;
        memory[3] = 0x00;

        memory[4] = (uint8_t)((entry >> 24) & 0xFFu);
        memory[5] = (uint8_t)((entry >> 16) & 0xFFu);
        memory[6] = (uint8_t)((entry >> 8) & 0xFFu);
        memory[7] = (uint8_t)(entry & 0xFFu);

        for (size_t i = 0; i < w->code_words; i++)
        {
            uint32_t addr = entry + (uint32_t)(i * 2);
            memory[addr] = (uint8_t)(w->code[i] >> 8);
            memory[addr + 1] = (uint8_t)(w->code[i] & 0xFFu);
        }

        BenchMoiraCPU cpu(memory);
        cpu.setModel(moira::Model::M68000);
        cpu.reset();
        cpu.setClock(0);

        cpu.setA(0, 0x8000);
        cpu.setSR(0x2700);

        const unsigned int stop_end_pc = (unsigned int)entry + find_stop_end_pc(w);
        const unsigned int stop_entry_pc = (stop_end_pc >= 2) ? (stop_end_pc - 2) : 0;

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        size_t slices = 0;
        for (;;)
        {
            cpu.execute(BENCH_SLICE_CYCLES);
            slices++;

            uint32_t pc = cpu.getPC();
            uint32_t d0 = cpu.getD(0);

            if (pc >= stop_entry_pc && d0 == 0) break;

            if ((slices & 0xFFu) == 0 &&
                bench_guard_exceeded("Moira", w->name, slices, &start))
            {
                return 1;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        print_result(w->name, time_diff(&start, &end), w->expected_iterations);
        return 0;
    }
} // namespace

int main(void)
{
    printf("Moira Benchmark\n");
    printf("===============\n");

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
