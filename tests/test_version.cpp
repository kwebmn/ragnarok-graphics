#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_36_0) { CHECK(std::strcmp(uagfx::kVersion, "0.36.0") == 0); }
