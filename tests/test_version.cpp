#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_23_3) { CHECK(std::strcmp(uagfx::kVersion, "0.23.3") == 0); }
