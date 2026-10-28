#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_9) { CHECK(std::strcmp(uagfx::kVersion, "3.0.9") == 0); }
