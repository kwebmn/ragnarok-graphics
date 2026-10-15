#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_54_0) { CHECK(std::strcmp(uagfx::kVersion, "0.54.0") == 0); }
