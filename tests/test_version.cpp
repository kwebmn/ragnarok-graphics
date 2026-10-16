#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_99_0) { CHECK(std::strcmp(uagfx::kVersion, "0.99.0") == 0); }
