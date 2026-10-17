#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_18_0) { CHECK(std::strcmp(uagfx::kVersion, "1.18.0") == 0); }
