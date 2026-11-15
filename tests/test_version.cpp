#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_19_beta_46) { CHECK(std::strcmp(uagfx::kVersion, "3.0.19-beta.46") == 0); }
