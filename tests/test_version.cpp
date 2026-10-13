#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_5_2) { CHECK(std::strcmp(uagfx::kVersion, "0.5.2") == 0); }
