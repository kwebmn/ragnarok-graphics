#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_35_0) { CHECK(std::strcmp(uagfx::kVersion, "0.35.0") == 0); }
