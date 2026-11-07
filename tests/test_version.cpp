#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_13_beta_28) { CHECK(std::strcmp(uagfx::kVersion, "3.0.13-beta.28") == 0); }
