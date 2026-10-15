#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_44_0) { CHECK(std::strcmp(uagfx::kVersion, "0.44.0") == 0); }
