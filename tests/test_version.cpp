#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_61_0) { CHECK(std::strcmp(uagfx::kVersion, "0.61.0") == 0); }
