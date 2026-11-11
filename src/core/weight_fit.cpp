#include "core/weight_fit.hpp"

namespace uagfx::weightfit {

int fits(int weight, int most, int thingTenths, int percent) {
    if (thingTenths <= 0 || most <= 0) return -1;
    const long long room = (long long)most * percent / 100 - weight - 1;  // the spare unit: the tenths unseen
    if (room <= 0) return 0;
    return int(room * 10 / thingTenths);
}

}  // namespace uagfx::weightfit
