#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_9_6) { CHECK(std::strcmp(uagfx::kVersion, "1.9.6") == 0); }
