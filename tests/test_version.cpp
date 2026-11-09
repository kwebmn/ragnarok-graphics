#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_18_beta_4) { CHECK(std::strcmp(uagfx::kVersion, "3.0.18-beta.4") == 0); }
