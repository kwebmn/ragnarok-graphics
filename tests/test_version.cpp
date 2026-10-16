#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_89_0) { CHECK(std::strcmp(uagfx::kVersion, "0.89.0") == 0); }
