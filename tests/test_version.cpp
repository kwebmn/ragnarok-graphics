#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_25_2) { CHECK(std::strcmp(uagfx::kVersion, "1.25.2") == 0); }
