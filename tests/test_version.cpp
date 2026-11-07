#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_15_beta_12) { CHECK(std::strcmp(uagfx::kVersion, "3.0.15-beta.12") == 0); }
