#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_13_beta_19) { CHECK(std::strcmp(uagfx::kVersion, "3.0.13-beta.19") == 0); }
