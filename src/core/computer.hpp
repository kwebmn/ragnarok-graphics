#pragma once
#include <cstdint>
#include <string>

namespace uagfx {

// 2.5.15: the computer besides its graphics card - the processor, the memory, the power - for the log and the report
// (an HD 6570's player's reports told only the card: how much the processor held the game back was a guess).
struct Computer {
    std::string cpu;  // the processor's own name (CPUID), "" unknown
    int cores = 0;    // physical cores, 0 unknown
    int threads = 0;  // logical processors
    unsigned mhz = 0;  // its rated clock (the registry's ~MHz), 0 unknown
    unsigned long long ramMb = 0, freeMb = 0;
    int mains = -1;    // 1 on the mains (a desktop too), 0 on battery, -1 unknown
    std::string plan;  // the power plan's name (powerPlanName), "" unknown
};

// The processor's name from CPUID 0x80000002-4's registers (eax, ebx, ecx, edx of each in turn): Intel pads it with
// spaces in front - trimmed, and a run of spaces made one.
std::string cpuBrand(const uint32_t regs[12]);
// Windows' own power plans by their GUID ("381b4222-f694-...", either case): their English names; "other" for one the
// user made, "" for none.
std::string powerPlanName(const std::string& guid);
// A line each for the log and the report; "unknown" when nothing is known.
// "Intel(R) Core(TM) i5-3470 CPU @ 3.20GHz, 4 cores, 4 threads, 3193 MHz"
std::string processorText(const Computer& m);
std::string memoryText(const Computer& m);  // "8148 MB, 4100 MB free"
std::string powerText(const Computer& m);   // "on the mains, plan Balanced"

}  // namespace uagfx
