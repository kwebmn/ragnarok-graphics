#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_97_0) { CHECK(std::strcmp(uagfx::kVersion, "0.97.0") == 0); }
