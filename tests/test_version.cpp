#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_26_1) { CHECK(std::strcmp(uagfx::kVersion, "0.26.1") == 0); }
