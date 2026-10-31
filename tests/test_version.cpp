#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_11_beta_109) { CHECK(std::strcmp(uagfx::kVersion, "3.0.11-beta.109") == 0); }
