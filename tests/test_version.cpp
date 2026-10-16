#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_94_0) { CHECK(std::strcmp(uagfx::kVersion, "0.94.0") == 0); }
