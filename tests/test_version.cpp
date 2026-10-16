#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_4_2) { CHECK(std::strcmp(uagfx::kVersion, "1.4.2") == 0); }
