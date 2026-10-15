#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_47_0) { CHECK(std::strcmp(uagfx::kVersion, "0.47.0") == 0); }
