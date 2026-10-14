#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_23_1) { CHECK(std::strcmp(uagfx::kVersion, "0.23.1") == 0); }
