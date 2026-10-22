#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace uagfx::app {

// 2.10: the files of uaro.kiev.ua's /files/graphics/ (core/update_check: kSite, kFiles) over WinHTTP; the tests'
// stand-in reads fake::Windows::web.
// `file` into `to` through "<to>.part" - a .part an earlier try left is continued (Range) - `progress(done, total)`
// returning false stops it. false and why, in English.
bool download(const std::string& file, const std::wstring& to,
              const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error);
// A small file's text (update.ini), at most 64 KB, never from a cache.
bool fetch(const std::string& file, std::string& out, std::string& error);
// 2.11: the text of a path of the site with its query (the launcher's answer), at most `most` bytes (2.13: a
// bigger answer is false).
bool fetchPath(const std::string& path, std::string& out, std::string& error, size_t most = 64u << 10);
// 2.13: a JSON body POSTed to a path of the site: its status and answer (at most 64 KB); false when no answer came.
bool postPath(const std::string& path, const std::string& body, long& status, std::string& answer,
              std::string& error);

}  // namespace uagfx::app
