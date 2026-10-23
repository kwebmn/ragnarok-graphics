#pragma once
#include <string>
#include <vector>

namespace uagfx::client_libs {

// 3.0.1-beta.5 (the user: our own libraries in place of the client's old ones; docs/superpowers/specs/
// 2026-10-23-own-client-libraries-design.md): the DLLs uaRO.exe takes from its folder that uaRO Graphics puts in -
// the package holds ours in uaro-graphics\clientlibs\, the game's own wait in uaro-graphics\original\.
struct Lib {
    const char* file;  // in the game's root: "cps.dll"
    const char* key;   // its switch, [client] <key>: "cps"
};
const std::vector<Lib>& all();
bool ours(const std::string& bytes);  // the marker our DLLs carry ("uaRO Graphics client library")

// The root's file of a library: none, ours (as the package's), ours of another build, or the game's own
enum class Root { Missing, Ours, OursOther, Game };
enum class Step { Nothing, PutOurs, RestoreOriginal };
// A sync's step for one library: `wanted` its switch, `haveOurs` the package holds it, `kept` the game's own waits
// aside. The root never lacks a library the exe needs: off with nothing kept, ours stays.
Step decide(bool wanted, bool haveOurs, Root root, bool kept);

// cps.dll's two: zlib's compress and uncompress, their semantics and codes (Z_OK 0, Z_DATA_ERROR -3,
// Z_BUF_ERROR -5), over miniz; *destLen the room in, the length out
int zlibCompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen);
int zlibUncompress(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen);

}  // namespace uagfx::client_libs
