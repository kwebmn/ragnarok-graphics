#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_91_0) { CHECK(std::strcmp(uagfx::kVersion, "0.91.0") == 0); }
