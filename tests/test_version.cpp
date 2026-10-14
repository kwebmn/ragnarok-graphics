#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_30_0) { CHECK(std::strcmp(uagfx::kVersion, "0.30.0") == 0); }
