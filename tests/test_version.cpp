#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_51_0) { CHECK(std::strcmp(uagfx::kVersion, "0.51.0") == 0); }
