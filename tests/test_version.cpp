#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_24_beta_33) { CHECK(std::strcmp(uagfx::kVersion, "3.0.24-beta.33") == 0); }
