#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_17_beta_9) { CHECK(std::strcmp(uagfx::kVersion, "3.0.17-beta.9") == 0); }
