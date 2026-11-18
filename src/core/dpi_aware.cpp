#include "core/dpi_aware.hpp"

namespace uagfx::dpiaware {

bool inexact(int width, int height, unsigned dpi) {
    if (dpi <= 96 || width <= 0 || height <= 0) return false;
    return (long long)width * 96 % dpi != 0 || (long long)height * 96 % dpi != 0;
}

bool wanted(DpiAware mode, int fullScreen, int width, int height, unsigned dpi) {
    if (mode == DpiAware::On) return true;
    if (mode == DpiAware::Off) return false;
    return fullScreen == 1 && inexact(width, height, dpi);
}

}  // namespace uagfx::dpiaware
