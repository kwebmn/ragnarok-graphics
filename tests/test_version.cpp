#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_22_beta_28) { CHECK(std::strcmp(uagfx::kVersion, "3.0.22-beta.28") == 0); }
