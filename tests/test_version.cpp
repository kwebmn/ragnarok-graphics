#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_5_5) { CHECK(std::strcmp(uagfx::kVersion, "1.5.5") == 0); }
