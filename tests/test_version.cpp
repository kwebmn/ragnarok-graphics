#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_9_0) { CHECK(std::strcmp(uagfx::kVersion, "0.9.0") == 0); }
