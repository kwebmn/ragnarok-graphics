#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_0_beta_3) { CHECK(std::strcmp(uagfx::kVersion, "3.0.0-beta.3") == 0); }
