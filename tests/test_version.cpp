#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_12_0) { CHECK(std::strcmp(uagfx::kVersion, "1.12.0") == 0); }
