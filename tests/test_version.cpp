#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_49_3) { CHECK(std::strcmp(uagfx::kVersion, "0.49.3") == 0); }
