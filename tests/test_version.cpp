#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_93_0) { CHECK(std::strcmp(uagfx::kVersion, "0.93.0") == 0); }
