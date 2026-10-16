#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_1_0) { CHECK(std::strcmp(uagfx::kVersion, "1.1.0") == 0); }
