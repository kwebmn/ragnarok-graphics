#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_96_0) { CHECK(std::strcmp(uagfx::kVersion, "0.96.0") == 0); }
