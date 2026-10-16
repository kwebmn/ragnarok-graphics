#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_95_0) { CHECK(std::strcmp(uagfx::kVersion, "0.95.0") == 0); }
