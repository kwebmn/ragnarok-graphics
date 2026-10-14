#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_11_3) { CHECK(std::strcmp(uagfx::kVersion, "0.11.3") == 0); }
