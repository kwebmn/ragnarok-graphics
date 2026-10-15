#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_88_0) { CHECK(std::strcmp(uagfx::kVersion, "0.88.0") == 0); }
