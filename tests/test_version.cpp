#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_68_0) { CHECK(std::strcmp(uagfx::kVersion, "0.68.0") == 0); }
