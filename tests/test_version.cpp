#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_17_0) { CHECK(std::strcmp(uagfx::kVersion, "0.17.0") == 0); }
