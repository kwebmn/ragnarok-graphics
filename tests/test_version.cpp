#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_23_0) { CHECK(std::strcmp(uagfx::kVersion, "1.23.0") == 0); }
