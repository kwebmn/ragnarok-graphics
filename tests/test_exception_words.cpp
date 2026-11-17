// 3.0.21-beta.14 (R-DP7PVN: the game ended a second after a picture's load, no line in the log - a C++ exception out
// of the driver's frame ends the game through std::terminate, which said nothing): what left, as the log says it
#include <exception>
#include <new>
#include <stdexcept>
#include <string>

#include "core/exception_words.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
template <typename F>
std::exception_ptr caught(F throwIt) {
    try {
        throwIt();
    } catch (...) {
        return std::current_exception();
    }
    return nullptr;
}
}  // namespace

TEST(exception_words_name_what_left_the_driver) {
    CHECK_EQ(exceptionWords(caught([] { throw std::out_of_range("vector::_M_range_check"); })),
             std::string("std::out_of_range: vector::_M_range_check"));
    CHECK_EQ(exceptionWords(caught([] { throw std::bad_alloc(); })), std::string("std::bad_alloc: std::bad_alloc"));
    CHECK_EQ(exceptionWords(caught([] { throw 5; })), std::string("an exception of type int"));
    CHECK_EQ(exceptionWords(nullptr), std::string("no exception (std::terminate called by itself)"));
}
