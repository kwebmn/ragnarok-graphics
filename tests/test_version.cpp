#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_82_0) { CHECK(std::strcmp(uagfx::kVersion, "0.82.0") == 0); }
