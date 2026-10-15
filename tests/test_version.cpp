#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_45_1) { CHECK(std::strcmp(uagfx::kVersion, "0.45.1") == 0); }
