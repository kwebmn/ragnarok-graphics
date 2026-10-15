#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_55_0) { CHECK(std::strcmp(uagfx::kVersion, "0.55.0") == 0); }
