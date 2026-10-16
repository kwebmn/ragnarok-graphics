#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_4_0) { CHECK(std::strcmp(uagfx::kVersion, "1.4.0") == 0); }
