#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_3_0) { CHECK(std::strcmp(uagfx::kVersion, "0.3.0") == 0); }
