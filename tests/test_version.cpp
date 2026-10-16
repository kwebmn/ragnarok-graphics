#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_3_5) { CHECK(std::strcmp(uagfx::kVersion, "1.3.5") == 0); }
