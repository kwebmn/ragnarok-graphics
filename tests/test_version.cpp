#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_56_0) { CHECK(std::strcmp(uagfx::kVersion, "0.56.0") == 0); }
