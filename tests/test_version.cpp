#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_86_0) { CHECK(std::strcmp(uagfx::kVersion, "0.86.0") == 0); }
