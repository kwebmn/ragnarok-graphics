#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_78_0) { CHECK(std::strcmp(uagfx::kVersion, "0.78.0") == 0); }
