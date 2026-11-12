#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_18_beta_215) { CHECK(std::strcmp(uagfx::kVersion, "3.0.18-beta.215") == 0); }
