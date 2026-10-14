#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_27_1) { CHECK(std::strcmp(uagfx::kVersion, "0.27.1") == 0); }
