#pragma once
#include <cstdint>
#include <string>

namespace uagfx::app {

// 0.53: the texture pack from the site into uaro-textures.zip (core/pack_download.hpp), on a thread of its own: the
// site's central directory by HTTP ranges, then only the pictures the local pack lacks, each checked, written as
// they come (a stop or a cut keeps what came; the next start goes on from there).
struct FetchState {
    enum Kind { Idle, Listing, Fetching, Done, Failed, Stopped } kind = Idle;
    uint64_t total = 0, done = 0;           // the bytes of the pictures to fetch, fetched
    uint32_t pictures = 0, fetched = 0;     // to fetch, fetched
    uint32_t already = 0;                   // in the local pack already
    double bytesPerSecond = 0;
    std::string error;                      // Failed: why, in English
};
void startFetch(const std::wstring& gameDir, const std::string& url);
void stopFetch();
FetchState fetchState();
bool fetchRunning();

}  // namespace uagfx::app
