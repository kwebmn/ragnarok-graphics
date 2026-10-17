#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_14_0) { CHECK(std::strcmp(uagfx::kVersion, "1.14.0") == 0); }
