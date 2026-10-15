#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_64_0) { CHECK(std::strcmp(uagfx::kVersion, "0.64.0") == 0); }
