#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_46_0) { CHECK(std::strcmp(uagfx::kVersion, "0.46.0") == 0); }
