#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_18_0) { CHECK(std::strcmp(uagfx::kVersion, "0.18.0") == 0); }
