#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_11_0) { CHECK(std::strcmp(uagfx::kVersion, "0.11.0") == 0); }
