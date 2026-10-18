#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_16) { CHECK(std::strcmp(uagfx::kVersion, "2.5.16") == 0); }
