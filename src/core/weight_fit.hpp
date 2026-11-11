#pragma once

namespace uagfx::weightfit {

// 3.0.18-beta.156 (the owner: how much can be carried counted ahead - potions by weight): how many of a thing go
// into the bag - the character's weight and its most in whole units (as the game keeps them: a tenth of the
// server's - one unit kept spare), the thing's in tenths (the item DB's: Red Potion 70); up to `percent` of the most;
// -1 no limit known (weightless, the most not known)
int fits(int weight, int most, int thingTenths, int percent = 100);

}  // namespace uagfx::weightfit
