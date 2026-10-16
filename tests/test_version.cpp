#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_92_0) { CHECK(std::strcmp(uagfx::kVersion, "0.92.0") == 0); }
