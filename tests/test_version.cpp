#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_16_beta_34) { CHECK(std::strcmp(uagfx::kVersion, "3.0.16-beta.34") == 0); }
