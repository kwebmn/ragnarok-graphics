#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_21_beta_8) { CHECK(std::strcmp(uagfx::kVersion, "3.0.21-beta.8") == 0); }
