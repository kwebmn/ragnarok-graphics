#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_67_0) { CHECK(std::strcmp(uagfx::kVersion, "0.67.0") == 0); }
