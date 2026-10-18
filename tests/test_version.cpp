#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_3_2) { CHECK(std::strcmp(uagfx::kVersion, "2.3.2") == 0); }
