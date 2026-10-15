#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_70_0) { CHECK(std::strcmp(uagfx::kVersion, "0.70.0") == 0); }
