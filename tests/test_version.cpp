#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_34_0) { CHECK(std::strcmp(uagfx::kVersion, "0.34.0") == 0); }
