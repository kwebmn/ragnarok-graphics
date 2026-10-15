#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_72_0) { CHECK(std::strcmp(uagfx::kVersion, "0.72.0") == 0); }
