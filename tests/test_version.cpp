#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_79_0) { CHECK(std::strcmp(uagfx::kVersion, "0.79.0") == 0); }
