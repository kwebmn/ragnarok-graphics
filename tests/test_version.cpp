#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_3_beta_8) { CHECK(std::strcmp(uagfx::kVersion, "3.0.3-beta.8") == 0); }
