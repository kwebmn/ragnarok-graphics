#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_2_4) { CHECK(std::strcmp(uagfx::kVersion, "2.2.4") == 0); }
