#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_23_beta_13) { CHECK(std::strcmp(uagfx::kVersion, "3.0.23-beta.13") == 0); }
