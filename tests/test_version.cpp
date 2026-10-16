#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_91_1) { CHECK(std::strcmp(uagfx::kVersion, "0.91.1") == 0); }
