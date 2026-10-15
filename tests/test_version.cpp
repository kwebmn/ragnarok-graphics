#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_85_0) { CHECK(std::strcmp(uagfx::kVersion, "0.85.0") == 0); }
