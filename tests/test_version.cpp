#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_66_0) { CHECK(std::strcmp(uagfx::kVersion, "0.66.0") == 0); }
