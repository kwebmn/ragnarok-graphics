#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_41_0) { CHECK(std::strcmp(uagfx::kVersion, "0.41.0") == 0); }
