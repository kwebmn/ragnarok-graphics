#include "app/launcher.hpp"

#include "app/download.hpp"

namespace uagfx::app {

LauncherState launcherNow(settings::Lang l) {
    LauncherState s;
    std::string text;
    if (!fetchPath(std::string(launcher::kPath) + "?lang=" + launcher::langToken(l), text, s.error) ||
        !launcher::parse(text, s.info, s.error)) {
        s.kind = LauncherState::Failed;
        return s;
    }
    s.kind = LauncherState::Done;
    return s;
}

}  // namespace uagfx::app
