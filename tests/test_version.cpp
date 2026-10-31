#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_11_beta_117) { CHECK(std::strcmp(uagfx::kVersion, "3.0.11-beta.117") == 0); }
