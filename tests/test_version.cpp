#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_2_2) { CHECK(std::strcmp(uagfx::kVersion, "2.2.2") == 0); }
