#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_4_3) { CHECK(std::strcmp(uagfx::kVersion, "0.4.3") == 0); }
