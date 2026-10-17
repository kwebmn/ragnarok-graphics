#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_19_0) { CHECK(std::strcmp(uagfx::kVersion, "1.19.0") == 0); }
