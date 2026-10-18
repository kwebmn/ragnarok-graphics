#include <algorithm>
#include <cstring>
#include <string>

#include "core/computer.hpp"
#include "core/settings/report.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// CPUID 0x80000002-4's registers for a name: 48 bytes, eax ebx ecx edx of each in turn, the rest zeros.
void brandRegs(const char* text, uint32_t regs[12]) {
    char bytes[48] = {};
    std::memcpy(bytes, text, std::min<size_t>(std::strlen(text), sizeof bytes));
    std::memcpy(regs, bytes, sizeof bytes);
}

}  // namespace

TEST(computer_reads_the_processors_name) {
    uint32_t regs[12];
    brandRegs("       Intel(R) Core(TM) i5-3470 CPU @ 3.20GHz", regs);  // Intel's: spaces in front
    CHECK_EQ(cpuBrand(regs), std::string("Intel(R) Core(TM) i5-3470 CPU @ 3.20GHz"));
    brandRegs("AMD Phenom(tm) II X4 965 Processor  ", regs);
    CHECK_EQ(cpuBrand(regs), std::string("AMD Phenom(tm) II X4 965 Processor"));
    brandRegs("AMD  Athlon   II X2", regs);  // runs of spaces made one
    CHECK_EQ(cpuBrand(regs), std::string("AMD Athlon II X2"));
    brandRegs("", regs);
    CHECK(cpuBrand(regs).empty());
}

TEST(computer_names_windows_power_plans) {
    CHECK_EQ(powerPlanName("381b4222-f694-41f0-9685-ff5bb260df2e"), std::string("Balanced"));
    CHECK_EQ(powerPlanName("8C5E7FDA-E8BF-4A96-9A85-A6E23A8C635C"), std::string("High performance"));
    CHECK_EQ(powerPlanName("a1841308-3541-4fab-bc81-f71556f20b4a"), std::string("Power saver"));
    CHECK_EQ(powerPlanName("e9a42b02-d5df-448d-aa00-03f14749eb61"), std::string("Ultimate Performance"));
    CHECK_EQ(powerPlanName("12345678-0000-0000-0000-000000000000"), std::string("other"));
    CHECK(powerPlanName("").empty());
}

TEST(computer_lines_for_the_log_and_the_report) {
    Computer m;
    m.cpu = "Intel(R) Core(TM) i5-3470 CPU @ 3.20GHz";
    m.cores = 4;
    m.threads = 4;
    m.mhz = 3193;
    m.ramMb = 8148;
    m.freeMb = 4100;
    m.mains = 1;
    m.plan = "Balanced";
    CHECK_EQ(processorText(m), std::string("Intel(R) Core(TM) i5-3470 CPU @ 3.20GHz, 4 cores, 4 threads, 3193 MHz"));
    CHECK_EQ(memoryText(m), std::string("8148 MB, 4100 MB free"));
    CHECK_EQ(powerText(m), std::string("on the mains, plan Balanced"));
    m.mains = 0;
    m.plan = "Power saver";
    CHECK_EQ(powerText(m), std::string("on battery, plan Power saver"));
    const Computer none;
    CHECK_EQ(processorText(none), std::string("unknown"));
    CHECK_EQ(memoryText(none), std::string("unknown"));
    CHECK_EQ(powerText(none), std::string("unknown"));
    Computer one;
    one.cpu = "AMD Athlon II X2 250";
    one.cores = 2;
    one.threads = 2;
    CHECK_EQ(processorText(one), std::string("AMD Athlon II X2 250, 2 cores, 2 threads"));
}

TEST(report_tells_the_processor_memory_and_power) {
    // 2.5.15: an HD 6570's player's reports had only the card - how much the processor held the game back was a guess
    settings::ReportFacts f;
    f.utilityVersion = "2.5.15";
    f.windows = "10.0 build 26200";
    f.processor = "AMD Athlon II X2 250, 2 cores, 2 threads, 3000 MHz";
    f.memory = "4096 MB, 1500 MB free";
    f.power = "on the mains, plan Balanced";
    const std::string r = settings::makeReport(f);
    const size_t windows = r.find("Windows: 10.0 build 26200\r\n");
    const size_t cpu = r.find("Processor: AMD Athlon II X2 250, 2 cores, 2 threads, 3000 MHz\r\n");
    CHECK(windows != std::string::npos && cpu != std::string::npos && windows < cpu);
    CHECK(r.find("Memory: 4096 MB, 1500 MB free\r\n") != std::string::npos);
    CHECK(r.find("Power: on the mains, plan Balanced\r\n") != std::string::npos);
    settings::ReportFacts old;  // an older settings app's facts: no lines of them
    CHECK(settings::makeReport(old).find("Processor:") == std::string::npos);
}
