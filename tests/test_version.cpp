#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_10_2) { CHECK(std::strcmp(uagfx::kVersion, "0.10.2") == 0); }
