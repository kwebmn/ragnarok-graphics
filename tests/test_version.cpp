#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_3) { CHECK(std::strcmp(uagfx::kVersion, "2.5.3") == 0); }
