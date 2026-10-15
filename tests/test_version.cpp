#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_81_0) { CHECK(std::strcmp(uagfx::kVersion, "0.81.0") == 0); }
