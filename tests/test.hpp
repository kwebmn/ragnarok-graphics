#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace t {
struct Case {
    const char* name;
    std::function<void()> fn;
};
std::vector<Case>& registry();
extern int failures;
struct Reg {
    Reg(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};
}  // namespace t

#define TEST(name)                         \
    static void name();                    \
    static t::Reg reg_##name(#name, name); \
    static void name()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++t::failures;                                                \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        auto _va = (a);                                                          \
        auto _vb = (b);                                                          \
        if (!(_va == _vb)) {                                                     \
            std::printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); \
            ++t::failures;                                                       \
        }                                                                        \
    } while (0)
