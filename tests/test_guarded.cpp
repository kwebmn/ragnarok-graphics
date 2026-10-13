#include <new>

#include "core/guarded.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(guarded_catches_what_the_work_throws) {
    int failures = 0;
    CHECK(!guarded([] { throw std::bad_alloc(); }, [&failures] { ++failures; }));
    CHECK_EQ(failures, 1);
    CHECK(!guarded([] { throw 1; }, [] { throw 2; }));  // nothing leaves it, not even from the handler
    int done = 0;
    CHECK(guarded([&done] { ++done; }, [&failures] { ++failures; }));
    CHECK_EQ(done, 1);
    CHECK_EQ(failures, 1);
}
