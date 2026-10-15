#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_47_1) { CHECK(std::strcmp(uagfx::kVersion, "0.47.1") == 0); }
