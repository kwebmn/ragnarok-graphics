#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_4_beta_31) { CHECK(std::strcmp(uagfx::kVersion, "3.0.4-beta.31") == 0); }
