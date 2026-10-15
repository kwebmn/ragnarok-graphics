#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_49_4) { CHECK(std::strcmp(uagfx::kVersion, "0.49.4") == 0); }
