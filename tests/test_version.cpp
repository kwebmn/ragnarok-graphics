#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_51_1) { CHECK(std::strcmp(uagfx::kVersion, "0.51.1") == 0); }
