#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_83_0) { CHECK(std::strcmp(uagfx::kVersion, "0.83.0") == 0); }
