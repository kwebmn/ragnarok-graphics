#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_76_0) { CHECK(std::strcmp(uagfx::kVersion, "0.76.0") == 0); }
