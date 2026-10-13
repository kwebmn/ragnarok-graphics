#include "core/blit.hpp"

namespace uagfx {

BlitAction routeBlit(const BlitRequest& r) {
    if (r.flags & ddblt::COLORFILL) {
        if (r.dst == Place::Cpu) return BlitAction::Fill;
        if (r.dst == Place::Target) return BlitAction::FillTarget;
        return BlitAction::Unsupported;
    }
    if (!r.hasSource) return BlitAction::Unsupported;
    if (r.dst == Place::Front && r.src == Place::Target) return BlitAction::Present;
    if (r.dst == Place::Cpu && r.src == Place::Cpu) return BlitAction::CopyCpu;
    if (r.dst == Place::Cpu && r.src == Place::Target) return BlitAction::ReadTarget;
    if (r.dst == Place::Target && r.src == Place::Cpu) return BlitAction::WriteTarget;
    return BlitAction::Unsupported;
}

}  // namespace uagfx
