#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_59_0) { CHECK(std::strcmp(uagfx::kVersion, "0.59.0") == 0); }
