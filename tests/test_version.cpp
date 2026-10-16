#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_5_1) { CHECK(std::strcmp(uagfx::kVersion, "1.5.1") == 0); }
