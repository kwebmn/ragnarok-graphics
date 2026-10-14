#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_19_0) { CHECK(std::strcmp(uagfx::kVersion, "0.19.0") == 0); }
