#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_22) { CHECK(std::strcmp(uagfx::kVersion, "2.5.22") == 0); }
