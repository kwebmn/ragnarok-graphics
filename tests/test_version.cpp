#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_26_0) { CHECK(std::strcmp(uagfx::kVersion, "0.26.0") == 0); }
