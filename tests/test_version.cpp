#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_45_0) { CHECK(std::strcmp(uagfx::kVersion, "0.45.0") == 0); }
