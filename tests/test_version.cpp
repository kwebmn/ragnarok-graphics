#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_25_2) { CHECK(std::strcmp(uagfx::kVersion, "0.25.2") == 0); }
