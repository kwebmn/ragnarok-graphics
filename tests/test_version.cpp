#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_2_beta_15) { CHECK(std::strcmp(uagfx::kVersion, "3.0.2-beta.15") == 0); }
