#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_11_4) { CHECK(std::strcmp(uagfx::kVersion, "0.11.4") == 0); }
