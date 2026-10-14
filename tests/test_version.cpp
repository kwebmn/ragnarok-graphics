#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_28_0) { CHECK(std::strcmp(uagfx::kVersion, "0.28.0") == 0); }
