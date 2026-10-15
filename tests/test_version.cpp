#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_77_0) { CHECK(std::strcmp(uagfx::kVersion, "0.77.0") == 0); }
