#pragma once
#include <dxgi.h>

#include <vector>

#include "core/gpu_order.hpp"

namespace uagfx::gpu {

// 0.50.1: the graphics cards to draw on, best first (core/gpu_order.hpp) - each AddRef'd: the caller releases them.
// all: every adapter Windows has, for the log.
std::vector<IDXGIAdapter1*> orderedAdapters(IDXGIFactory1* factory, std::vector<AdapterFacts>* all = nullptr);
void releaseAdapters(std::vector<IDXGIAdapter1*>& adapters);

}  // namespace uagfx::gpu
