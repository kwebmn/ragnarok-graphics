#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_14_2) { CHECK(std::strcmp(uagfx::kVersion, "0.14.2") == 0); }
