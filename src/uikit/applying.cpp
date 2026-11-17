#include "uikit/applying.hpp"

namespace uagfx::uikit {
namespace {
int g_applying = 0;  // the interface runs on the game's thread alone
}

Applying::Applying() { ++g_applying; }
Applying::~Applying() { --g_applying; }
bool applying() { return g_applying > 0; }

}  // namespace uagfx::uikit
