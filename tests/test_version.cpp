#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_4_1) { CHECK(std::strcmp(uagfx::kVersion, "0.4.1") == 0); }
