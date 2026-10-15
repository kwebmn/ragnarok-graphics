#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_73_0) { CHECK(std::strcmp(uagfx::kVersion, "0.73.0") == 0); }
