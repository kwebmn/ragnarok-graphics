#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_0) { CHECK(std::strcmp(uagfx::kVersion, "2.7.0") == 0); }
