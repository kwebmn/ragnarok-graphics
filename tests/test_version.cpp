#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_46_1) { CHECK(std::strcmp(uagfx::kVersion, "0.46.1") == 0); }
