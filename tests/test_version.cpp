#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_53_0) { CHECK(std::strcmp(uagfx::kVersion, "0.53.0") == 0); }
