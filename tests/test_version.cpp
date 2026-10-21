#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_46) { CHECK(std::strcmp(uagfx::kVersion, "2.7.46") == 0); }
