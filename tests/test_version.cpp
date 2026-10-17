#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_7_3) { CHECK(std::strcmp(uagfx::kVersion, "1.7.3") == 0); }
