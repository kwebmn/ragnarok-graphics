// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

namespace {

const IniSetting& setting(const char* sectionKey) { return *findSetting(sectionKey); }

int effectiveDword(const char* name) {
    const auto v = effectiveValue(fake::windows().registry, "", name);
    return v && v->asDword() ? int(*v->asDword()) : -1;
}

}  // namespace

TEST(model_reads_the_client_folder_and_the_game_key) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 70\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK_EQ(m.token(setting("quality.sharpen")), std::string("70"));
    CHECK(m.backend() == Backend::Uaro);
    CHECK_EQ(m.pendingCount(), size_t(0));
    CHECK(m.screenNow().game == (Size{1920, 1080}));
    CHECK(m.screenNow().choice.window == (Size{3360, 1890}));
}
