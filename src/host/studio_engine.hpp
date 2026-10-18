#pragma once
#include <string>

namespace uagfx::host {

// 2.3, uaRO Studio's engine: uaro-render64.exe --studio <pipe> --dir "<game>" - a copy of the installed renderer the
// studio runs from its own folder (the driver can be updated meanwhile). It connects to the studio's named pipe
// (\\.\pipe\<pipe>) and answers its requests (core/studio_link.hpp), one at a time: hello; enlarge {in, out, model,
// denoise, border} - a raw picture (studio::rawOf) 4 times as large by a model, into the file `out`; build {mode,
// kinds, ai, quality, fresh, map, mapKinds, remake, colours, catalog} - a texture pack build, its status as progress
// replies, a stop heard between its pictures; quit. The card and the networks are taken at the first picture; it ends
// when the pipe breaks.
int runStudio(const std::string& gameDir, const std::string& pipe);

}  // namespace uagfx::host
