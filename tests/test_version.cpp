#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_26) { CHECK(std::strcmp(uagfx::kVersion, "2.5.26") == 0); }
