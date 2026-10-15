#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_52_0) { CHECK(std::strcmp(uagfx::kVersion, "0.52.0") == 0); }
