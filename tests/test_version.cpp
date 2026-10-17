#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_9_3) { CHECK(std::strcmp(uagfx::kVersion, "1.9.3") == 0); }
