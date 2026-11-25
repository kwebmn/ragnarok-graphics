#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_23_beta_9) { CHECK(std::strcmp(uagfx::kVersion, "3.0.23-beta.9") == 0); }
