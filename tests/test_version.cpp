#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_7_0) { CHECK(std::strcmp(uagfx::kVersion, "0.7.0") == 0); }
