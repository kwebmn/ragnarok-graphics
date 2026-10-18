#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_2_3) { CHECK(std::strcmp(uagfx::kVersion, "2.2.3") == 0); }
