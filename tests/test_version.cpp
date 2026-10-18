#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_4_0) { CHECK(std::strcmp(uagfx::kVersion, "2.4.0") == 0); }
