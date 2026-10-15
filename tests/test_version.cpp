#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_45_2) { CHECK(std::strcmp(uagfx::kVersion, "0.45.2") == 0); }
