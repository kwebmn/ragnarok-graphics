#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "test.hpp"

namespace {

const char* g_running = nullptr;

// A crashing test would otherwise take the whole report with it: name it and fail the run.
void onCrash(int sig) {
    std::printf("CRASH %s (signal %d)\n", g_running ? g_running : "?", sig);
    std::fflush(stdout);
    _exit(3);
}

}  // namespace

namespace t {
std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}
int failures = 0;
}  // namespace t

int main(int argc, char** argv) {
    std::signal(SIGSEGV, onCrash);
    std::signal(SIGABRT, onCrash);
    std::signal(SIGFPE, onCrash);
    const char* filter = argc > 1 ? argv[1] : "";
    int ran = 0;
    for (const t::Case& c : t::registry()) {
        if (*filter && !std::strstr(c.name, filter)) continue;
        const int before = t::failures;
        g_running = c.name;
        c.fn();
        ++ran;
        std::printf("%s %s\n", t::failures == before ? "PASS" : "FAIL", c.name);
        std::fflush(stdout);
    }
    std::printf("%d tests, %d failed checks\n", ran, t::failures);
    if (ran == 0) {
        std::printf("no tests match '%s'\n", filter);
        return 2;
    }
    return t::failures ? 1 : 0;
}
