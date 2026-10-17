#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_25_0) { CHECK(std::strcmp(uagfx::kVersion, "1.25.0") == 0); }
