#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_63_0) { CHECK(std::strcmp(uagfx::kVersion, "0.63.0") == 0); }
