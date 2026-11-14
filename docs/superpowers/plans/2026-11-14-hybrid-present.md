# Laptops with two cards: the frames to the screen (R-T8FGMM)

Owner 2026-11-14: «делай оба шага, а потом 3д режим». R-T8FGMM: AMD R7 M370 draws (D3D12), the built-in screen hangs
off Intel HD 520 - the swap chain's present 201 ms every frame (a fixed ~200 ms: a timeout), 5 fps whatever the effects.

Step 1 (3.0.19-beta.8): core/gpu_timing presentMs/cpuMs (render64 noteGpuFrame from swapTicks/cpuTicks; D3D11 around
its Present); core/present_watch (kSlowMs 100, kRun 20 half seconds, the card's work under half the present) +
tests; win/present_watch (the world in front: feed; once: D3D12 -> [renderer] api = d3d11, D3D11 -> [renderer] gpu =
the screen's card; the chat told, Text::PresentSlowD3d11/Card; the cards logged: screenCard() by DXGI outputs vs
renderer.adapterName()).

Step 2 (next): Direct3D 12 presents through the screen's card - render on the fast card into its own picture, copy
it into a cross-adapter shared buffer (D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER, a shared fence), the screen's card copies
it into its swap chain's back buffer and presents (Microsoft's heterogeneous multi-adapter scheme).

## Ledger
Step 1: code complete, host 2774/2774, win32 + win64 build clean.
