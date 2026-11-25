#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_23_beta_5) { CHECK(std::strcmp(uagfx::kVersion, "3.0.23-beta.5") == 0); }
