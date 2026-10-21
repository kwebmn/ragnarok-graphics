#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_48) { CHECK(std::strcmp(uagfx::kVersion, "2.7.48") == 0); }
