#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_2_beta_6) { CHECK(std::strcmp(uagfx::kVersion, "3.0.2-beta.6") == 0); }
