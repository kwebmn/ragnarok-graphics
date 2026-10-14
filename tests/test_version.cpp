#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_37_0) { CHECK(std::strcmp(uagfx::kVersion, "0.37.0") == 0); }
