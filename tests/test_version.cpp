#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_43_0) { CHECK(std::strcmp(uagfx::kVersion, "0.43.0") == 0); }
