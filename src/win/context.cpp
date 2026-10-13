#include "win/context.hpp"

#include "win/log.hpp"
#include "win/window_mode.hpp"

namespace uagfx::win {

Context& Context::get() {
    static Context c;
    return c;
}

Context::Context() {
    InitializeCriticalSection(&cs_);
    screenWidth = GetSystemMetrics(SM_CXSCREEN);  // what a windowed client sees until SetDisplayMode
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

bool Context::open(std::string& error) {
    if (renderer) return true;
    gpu::Renderer* r = new gpu::Renderer;
    if (!r->create(error)) {
        delete r;
        return false;
    }
    renderer = r;
    renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    LOG_INFO("Direct3D 12 renderer on %s", renderer->adapterName().c_str());
    return true;
}

void Context::close() {
    flush();
    logStats(true);
    delete renderer;  // waits for the GPU
    renderer = nullptr;
    record.reset();
    window = nullptr;
    windowApplied = false;
}

bool Context::setTarget(int width, int height) {
    if (!renderer || !renderer->setTarget(width, height)) return false;
    record.reset();
    installShadowHandler();
    if (!shadow.resize(width, height, screenFormat)) {
        LOG_ERROR("cannot allocate the %dx%d CPU copy of the target", width, height);
        return false;
    }
    return true;
}

void Context::flush() {
    if (!renderer || record.empty()) return;
    renderer->execute(record);
    record.reset();
    shadow.gpuChanged();
}

bool Context::readTarget(std::vector<uint32_t>& argb) {
    flush();
    ++stats_.readBacks;
    return renderer && renderer->readTarget(argb);
}

void Context::writeTarget(const Rect& rect, const uint32_t* argb) {
    record.writeTarget(rect, argb);
    flushIfFull();
}

void Context::flushIfFull() {
    if (record.full()) flush();
}

void Context::present(const Rect& source, bool waitForVblank) {
    flush();
    if (!renderer) return;
    if (window && IsIconic(window)) {  // minimised: nothing to show, and no vsync to hold the game's loop back
        Sleep(15);
        return;
    }
    if (!windowApplied && window) {
        // Not earlier: a windowed client sizes its back buffer from the window's client area (L3039), and that
        // logical size must stay the one from its settings, whatever the window becomes. The flag goes first:
        // SetWindowPos sends WM_MOVE, and the client may present again from its handler (L457929).
        windowApplied = true;
        applyWindowMode(window, config, gameFullscreen, shadow.width(), shadow.height());
    }
    gpu::PresentOptions o;
    o.vsync = config.vsync && waitForVblank;
    o.allowTearing = config.allowTearing;
    o.fpsLimit = config.fpsLimit;
    renderer->present(window, source, o);
    ++stats_.frames;
    logStats(false);
}

void Context::logStats(bool final) {
    const DWORD now = GetTickCount();
    if (!stats_.since) stats_.since = now;
    const DWORD ms = now - stats_.since;
    if (!final && ms < 30000) return;
    if (stats_.frames || stats_.readBacks)
        LOG_INFO("%u frames in %.1f s (%.1f fps), %u reads of the frame from the GPU", stats_.frames, ms / 1000.0,
                 ms ? stats_.frames * 1000.0 / ms : 0.0, stats_.readBacks);
    stats_ = Stats{};
    stats_.since = now;
}

}  // namespace uagfx::win
