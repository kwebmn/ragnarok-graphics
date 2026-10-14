#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_12_0) { CHECK(std::strcmp(uagfx::kVersion, "0.12.0") == 0); }
