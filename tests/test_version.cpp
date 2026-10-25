#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_5_beta_14) { CHECK(std::strcmp(uagfx::kVersion, "3.0.5-beta.14") == 0); }
