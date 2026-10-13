#include "core/blit.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
BlitAction route(Place dst, Place src, uint32_t flags, bool hasSource = true) {
    BlitRequest r;
    r.dst = dst;
    r.src = src;
    r.hasSource = hasSource;
    r.flags = flags;
    return routeBlit(r);
}
}  // namespace

TEST(blit_present_paths) {
    CHECK(route(Place::Front, Place::Target, ddblt::WAIT) == BlitAction::Present);  // windowed present, Bink
}

TEST(blit_colour_fill) {
    CHECK(route(Place::Cpu, Place::Cpu, ddblt::COLORFILL | ddblt::WAIT, false) == BlitAction::Fill);
    CHECK(route(Place::Target, Place::Cpu, ddblt::COLORFILL | ddblt::WAIT, false) == BlitAction::FillTarget);
    CHECK(route(Place::Front, Place::Cpu, ddblt::COLORFILL, false) == BlitAction::Unsupported);
}

TEST(blit_between_memory_and_the_target) {
    CHECK(route(Place::Cpu, Place::Target, ddblt::WAIT) == BlitAction::ReadTarget);  // calibration, fade save
    CHECK(route(Place::Target, Place::Cpu, ddblt::WAIT) == BlitAction::WriteTarget);  // fade restore, big pictures
    CHECK(route(Place::Cpu, Place::Cpu, ddblt::ASYNC) == BlitAction::CopyCpu);         // mip levels, glyphs
}

TEST(blit_what_the_client_never_does) {
    CHECK(route(Place::Target, Place::Target, ddblt::WAIT) == BlitAction::Unsupported);
    CHECK(route(Place::Cpu, Place::Front, ddblt::WAIT) == BlitAction::Unsupported);
    CHECK(route(Place::Cpu, Place::Cpu, ddblt::WAIT, false) == BlitAction::Unsupported);  // no source, no fill
}
