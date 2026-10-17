#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_1_7) { CHECK(std::strcmp(uagfx::kVersion, "2.1.7") == 0); }
