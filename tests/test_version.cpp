#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_4_2) { CHECK(std::strcmp(uagfx::kVersion, "2.4.2") == 0); }
