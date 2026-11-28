#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_24_beta_38) { CHECK(std::strcmp(uagfx::kVersion, "3.0.24-beta.38") == 0); }
