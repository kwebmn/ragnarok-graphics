#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_2) { CHECK(std::strcmp(uagfx::kVersion, "2.5.2") == 0); }
