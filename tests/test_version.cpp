#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_14_0) { CHECK(std::strcmp(uagfx::kVersion, "0.14.0") == 0); }
