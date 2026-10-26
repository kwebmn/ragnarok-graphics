#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_7_beta_1) { CHECK(std::strcmp(uagfx::kVersion, "3.0.7-beta.1") == 0); }
