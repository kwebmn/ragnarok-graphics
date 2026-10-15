#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_49_1) { CHECK(std::strcmp(uagfx::kVersion, "0.49.1") == 0); }
