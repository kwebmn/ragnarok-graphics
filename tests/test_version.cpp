#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_3_2) { CHECK(std::strcmp(uagfx::kVersion, "1.3.2") == 0); }
