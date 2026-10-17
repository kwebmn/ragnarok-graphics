#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_11_0) { CHECK(std::strcmp(uagfx::kVersion, "1.11.0") == 0); }
