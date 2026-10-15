#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_69_0) { CHECK(std::strcmp(uagfx::kVersion, "0.69.0") == 0); }
