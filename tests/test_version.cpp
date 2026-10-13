#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_5_0) { CHECK(std::strcmp(uagfx::kVersion, "0.5.0") == 0); }
