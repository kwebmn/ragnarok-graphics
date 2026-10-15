#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_71_0) { CHECK(std::strcmp(uagfx::kVersion, "0.71.0") == 0); }
