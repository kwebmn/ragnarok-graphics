#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_58_0) { CHECK(std::strcmp(uagfx::kVersion, "0.58.0") == 0); }
