#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_50_2) { CHECK(std::strcmp(uagfx::kVersion, "0.50.2") == 0); }
