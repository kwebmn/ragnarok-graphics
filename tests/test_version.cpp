#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_14) { CHECK(std::strcmp(uagfx::kVersion, "3.0.14") == 0); }
