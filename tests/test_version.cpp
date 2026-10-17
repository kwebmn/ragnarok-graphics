#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_17_0) { CHECK(std::strcmp(uagfx::kVersion, "1.17.0") == 0); }
