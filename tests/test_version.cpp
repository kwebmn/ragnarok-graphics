#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_23_2) { CHECK(std::strcmp(uagfx::kVersion, "0.23.2") == 0); }
