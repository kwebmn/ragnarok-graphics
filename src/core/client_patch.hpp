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
// 3.0.1: the client build's own manifest in the game's root (format 1 too): which client this folder is - «uaRO HD»
// stands next to the old uaRO, each with its own list on the site - and what its build put. None: the old «uaRO».
constexpr const char* kClientFile = "uaro-client.json";
constexpr const char* kOldClient = "uaRO";

struct File {
    std::string path, sha256;  // path with '/'
    uint64_t size = 0;
    bool made = false;  // 3.0.1, the record's: the patch made it (nothing was at its place) - it may go again
};
struct Manifest {
    std::string client, version;  // 3.0.1: client - whose list or build
    std::vector<File> files;
};
// false and why for what is not format 1; an unsafe entry is dropped, not the manifest
bool parse(const std::string& json, Manifest& out, std::string& error);
bool safePath(const std::string& path);
std::string localPath(const std::string& path);  // '/' -> '\'

// 3.0.1: a client's name fit for a shortcut's file and a key of Windows' Apps - 1 to 40 of letters, digits, space,
// '-', '_', '.', '(', ')', a letter or a digit first, no space or '.' last, no name Windows keeps (CON...) - else
// the old «uaRO»; the same client in any case ("" is the old one)
std::string clientName(const std::string& name);
bool sameClient(const std::string& a, const std::string& b);
// 3.0.1: the files a player changes on purpose (the client build's report): Palettes.grf («Палітри гри» makes a
// smaller one) and dinput8.ini (ROExt's window) in the game's root, any case
bool playersOwn(const std::string& path);

// A listed file on the disk: `sha256` "" when it was not computed (the size is trusted then)
struct Have {
    bool exists = false;
    uint64_t size = 0;
    std::string sha256;
};
// The file at `path`, its sha256 computed only when its size is one of `sizes`
using Look = std::function<Have(const std::string& path, const std::vector<uint64_t>& sizes)>;
// 3.0.1: what the list `next` asks of the folder after `last` (the record of the last put) on the build's files
struct Plan {
    std::vector<File> put;     // missing, another file - the player's own only while as the build or the patch left it
    std::vector<File> kept;    // the player's own, changed by them: left as they are
    std::vector<File> remove;  // the last list's, gone from this one: made by the patch, unchanged, not the build's
};
Plan plan(const Manifest& next, const Manifest& last, const Manifest& build, const Look& look);
void carryMade(Manifest& next, const Manifest& last);  // each file's `made` as the last record had it

// uaro-graphics\client-patch.txt: the list put ("version", "client", then "<size> <sha256> made|over <path>") - the
// next check hashes nothing while the same list is there with every file of its size (`sizeOf`: -1 missing; the
// player's own need only be there)
std::string record(const Manifest& m);
bool readRecord(const std::string& text, Manifest& out);  // also 3.0.0's lines "<size> <sha256> <path>"
bool upToDate(const Manifest& m, const std::string& recordText, const std::function<long long(const std::string&)>&
                                                                    sizeOf);

}  // namespace uagfx::client_patch
