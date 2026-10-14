#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_38_0) { CHECK(std::strcmp(uagfx::kVersion, "0.38.0") == 0); }
