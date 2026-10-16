#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_3_4) { CHECK(std::strcmp(uagfx::kVersion, "1.3.4") == 0); }
