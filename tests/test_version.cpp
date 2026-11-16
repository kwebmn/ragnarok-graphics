#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_20_beta_61) { CHECK(std::strcmp(uagfx::kVersion, "3.0.20-beta.61") == 0); }
