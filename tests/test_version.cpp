#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_5_1) { CHECK(std::strcmp(uagfx::kVersion, "0.5.1") == 0); }
