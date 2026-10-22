#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace uagfx::app {

// HTTPS over WinHTTP: uaro.kiev.ua's paths (core/update_check kSite) and, 3.0.0-beta.2, the updates' files of GitHub
// (any "https://host/path"); the tests' stand-in reads fake::Windows::web.
// `url` into `to` through "<to>.part" - a .part an earlier try left is continued (Range) - `progress(done, total)`
// returning false stops it. false and why, in English (the host named).
bool downloadUrl(const std::string& url, const std::wstring& to,
                 const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error);
// 2.14: the same for a file of uaro.kiev.ua (the client patch's /files/client/<sha256>)
bool downloadPath(const std::string& path, const std::wstring& to,
                  const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error);
// A small file's text (update.ini), at most `most` bytes.
bool fetchUrl(const std::string& url, std::string& out, std::string& error, size_t most = 64u << 10);
// 2.11: the text of a path of the site with its query (the launcher's answer), at most `most` bytes (2.13: a
// bigger answer is false).
bool fetchPath(const std::string& path, std::string& out, std::string& error, size_t most = 64u << 10);
// 2.13: a JSON body POSTed to a path of the site: its status and answer (at most 64 KB); false when no answer came.
bool postPath(const std::string& path, const std::string& body, long& status, std::string& answer,
              std::string& error);

}  // namespace uagfx::app
