#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_7_beta_2) { CHECK(std::strcmp(uagfx::kVersion, "3.0.7-beta.2") == 0); }
