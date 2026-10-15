#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_83_1) { CHECK(std::strcmp(uagfx::kVersion, "0.83.1") == 0); }
