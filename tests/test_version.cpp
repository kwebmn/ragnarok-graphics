#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_9_7) { CHECK(std::strcmp(uagfx::kVersion, "1.9.7") == 0); }
