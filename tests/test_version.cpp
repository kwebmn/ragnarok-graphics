#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_13_beta_24) { CHECK(std::strcmp(uagfx::kVersion, "3.0.13-beta.24") == 0); }
