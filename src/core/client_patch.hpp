#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uagfx::client_patch {

// 2.14: the client patcher - the game's files the admin put in «Оновлення клієнта» on uaro.kiev.ua. The manifest is
// the client build's shared format 1 (/root/uaro-client-build/CLAUDE.md §6): a path from the game's folder with '/',
// its size, its sha256; the files themselves are /files/client/<sha256>. Never DATA.INI, uaRO Graphics' own files,
// the player's or programs - the site refuses them and so does this.
constexpr const char* kPath = "/api/public/client-patch";
constexpr const char* kFiles = "/files/client/";

struct File {
    std::string path, sha256;  // path with '/'
    uint64_t size = 0;
};
struct Manifest {
    std::string version;
    std::vector<File> files;
};
// false and why for what is not format 1; an unsafe entry is dropped, not the manifest
bool parse(const std::string& json, Manifest& out, std::string& error);
bool safePath(const std::string& path);
std::string localPath(const std::string& path);  // '/' -> '\'

// A listed file on the disk: `sha256` "" when it was not computed (the size is trusted then)
struct Have {
    bool exists = false;
    uint64_t size = 0;
    std::string sha256;
};
std::vector<File> needed(const Manifest& m, const std::function<Have(const File&)>& have);

// uaro-graphics\client-patch.txt: the version put and its files - the next check hashes nothing while the same list
// is there with every file of its size (`sizeOf`: -1 missing)
std::string record(const Manifest& m);
bool upToDate(const Manifest& m, const std::string& recordText, const std::function<long long(const std::string&)>&
                                                                    sizeOf);

}  // namespace uagfx::client_patch
