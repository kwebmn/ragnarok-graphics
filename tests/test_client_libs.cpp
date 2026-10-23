// 3.0.1-beta.5: our own libraries in place of the client's old ones (core/client_libs) - which they are, what a
// sync does with each in the game's root, and cps.dll's zlib.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/client_libs.hpp"
#include "test.hpp"

using namespace uagfx;
using client_libs::Root;
using client_libs::Step;

TEST(client_libs_know_the_libraries) {
    const auto& all = client_libs::all();
    CHECK_EQ(all.size(), size_t(5));
    CHECK(std::string(all[2].file) == "ijl15.dll" && std::string(all[2].key) == "ijl");  // 3.0.1-beta.6
    // 3.0.1-beta.7: Miles over XAudio2 2.9 - put in only where Windows has it
    CHECK(std::string(all[3].file) == "mss32.dll" && std::string(all[3].key) == "mss" && all[3].needsXAudio2);
    CHECK(!all[0].needsXAudio2 && !all[1].needsXAudio2 && !all[2].needsXAudio2);
    // 3.0.1-beta.8: ROExt's dinput8.dll - no file of ours, the driver does its work: the game's goes aside
    CHECK(std::string(all[4].file) == "dinput8.dll" && std::string(all[4].key) == "roext" && all[4].aside);
    CHECK(!all[0].aside && !all[3].aside);
    CHECK(std::string(all[0].file) == "cps.dll" && std::string(all[0].key) == "cps");
    CHECK(std::string(all[1].file) == "binkw32.dll" && std::string(all[1].key) == "bink");
    CHECK(client_libs::ours(std::string("MZ\x90\0 uaRO Graphics client library: cps.dll", 43)));
    CHECK(!client_libs::ours("MZ RAD Game Tools binkw32"));
}

// The root never lacks a library the exe needs; the game's own goes aside, comes back when ours is off
TEST(client_libs_decide_each_step) {
    CHECK(client_libs::decide(true, true, Root::Game, false) == Step::PutOurs);       // the game's aside, ours in
    CHECK(client_libs::decide(true, true, Root::Game, true) == Step::PutOurs);        // a newer game's file: kept
    CHECK(client_libs::decide(true, true, Root::Missing, false) == Step::PutOurs);
    CHECK(client_libs::decide(true, true, Root::Ours, true) == Step::Nothing);
    CHECK(client_libs::decide(true, true, Root::OursOther, true) == Step::PutOurs);   // another build of ours
    CHECK(client_libs::decide(false, true, Root::Ours, true) == Step::RestoreOriginal);
    CHECK(client_libs::decide(false, true, Root::OursOther, true) == Step::RestoreOriginal);
    CHECK(client_libs::decide(false, true, Root::Ours, false) == Step::Nothing);      // nothing to put back
    CHECK(client_libs::decide(false, true, Root::Missing, true) == Step::RestoreOriginal);
    CHECK(client_libs::decide(false, true, Root::Game, true) == Step::Nothing);
    CHECK(client_libs::decide(true, false, Root::Ours, true) == Step::RestoreOriginal);  // ours not in the package
    CHECK(client_libs::decide(true, false, Root::Game, false) == Step::Nothing);
}

// cps.dll: zlib's compress and uncompress, its codes (Z_OK 0, Z_DATA_ERROR -3, Z_BUF_ERROR -5)
TEST(client_libs_zlib_is_zlibs) {
    const unsigned char zlibStream[] = "\x78\xda\x2b\x4d\x0c\xf2\x57\x70\x0f\x72\x53\x48\xcd\x2b\x29\xaa\xb4\x52\x28"
                                       "\x28\xca\xcf\x2b\x49\x2d\x4a\x1c\x4a\x0c\x00\xf4\x65\x4b\xff";
    std::string want = "uaRO GRF entry: ";
    for (int i = 0; i < 20; ++i) want += "prontera ";
    std::vector<unsigned char> out(want.size());
    unsigned long n = (unsigned long)out.size();
    CHECK_EQ(client_libs::zlibUncompress(out.data(), &n, zlibStream, 34), 0);  // a stream of zlib itself
    CHECK(n == want.size() && std::memcmp(out.data(), want.data(), n) == 0);
    std::vector<unsigned char> packed(want.size() + 64);
    unsigned long p = (unsigned long)packed.size();
    CHECK_EQ(client_libs::zlibCompress(packed.data(), &p, reinterpret_cast<const unsigned char*>(want.data()),
                                       (unsigned long)want.size()), 0);
    std::vector<unsigned char> back(want.size());
    unsigned long b = (unsigned long)back.size();
    CHECK_EQ(client_libs::zlibUncompress(back.data(), &b, packed.data(), p), 0);
    CHECK(b == want.size() && std::memcmp(back.data(), want.data(), b) == 0);
    unsigned long small = 10;
    CHECK_EQ(client_libs::zlibUncompress(back.data(), &small, zlibStream, 34), -5);  // no room
    const unsigned char garbage[] = "this is no zlib stream";
    unsigned long g = (unsigned long)back.size();
    CHECK_EQ(client_libs::zlibUncompress(back.data(), &g, garbage, sizeof garbage), -3);
}

// 3.0.2-beta.11 (cps.dll on libdeflate): as zlib's uncompress in what the game may pass - room to spare (the length
// it took out), bytes after the stream (not read), a stream of many blocks, a damaged check (Adler-32)
TEST(client_libs_zlib_as_the_game_calls_it) {
    std::vector<unsigned char> data(3u << 20);
    uint32_t x = 12345;
    for (size_t i = 0; i < data.size(); ++i) {  // a ground file's mix: runs and noise
        x = x * 1103515245u + 12345u;
        data[i] = (i / 4096) % 3 ? static_cast<unsigned char>(x >> 24) : static_cast<unsigned char>(i / 64);
    }
    std::vector<unsigned char> packed(data.size() + data.size() / 100 + 1024);
    unsigned long p = (unsigned long)packed.size();
    CHECK_EQ(client_libs::zlibCompress(packed.data(), &p, data.data(), (unsigned long)data.size()), 0);
    packed.resize(p);
    std::vector<unsigned char> out(data.size() + 100);
    unsigned long n = (unsigned long)out.size();
    CHECK_EQ(client_libs::zlibUncompress(out.data(), &n, packed.data(), p), 0);
    CHECK(n == data.size() && std::memcmp(out.data(), data.data(), n) == 0);
    std::vector<unsigned char> tail = packed;
    tail.insert(tail.end(), {0, 0, 0, 0, 0, 0, 0, 0});
    n = (unsigned long)out.size();
    CHECK_EQ(client_libs::zlibUncompress(out.data(), &n, tail.data(), (unsigned long)tail.size()), 0);
    CHECK(n == data.size());
    std::vector<unsigned char> broken = packed;
    broken[broken.size() - 1] ^= 0x5A;  // the stream's Adler-32
    n = (unsigned long)out.size();
    CHECK_EQ(client_libs::zlibUncompress(out.data(), &n, broken.data(), (unsigned long)broken.size()), -3);
    unsigned long none = 1;  // no room for a compressed stream
    CHECK_EQ(client_libs::zlibCompress(out.data(), &none, data.data(), (unsigned long)data.size()), -5);
}

// 3.0.1-beta.8: a library the driver does the work of (ROExt): on - the game's aside, nothing in its place; off - back
TEST(client_libs_decide_a_library_put_aside) {
    CHECK(client_libs::decideAside(true, Root::Game, false) == Step::PutAside);
    CHECK(client_libs::decideAside(true, Root::Game, true) == Step::PutAside);  // a newer one of the game's: aside too
    CHECK(client_libs::decideAside(true, Root::Missing, true) == Step::Nothing);
    CHECK(client_libs::decideAside(false, Root::Missing, true) == Step::RestoreOriginal);
    CHECK(client_libs::decideAside(false, Root::Missing, false) == Step::Nothing);
    CHECK(client_libs::decideAside(false, Root::Game, true) == Step::Nothing);  // the game's there: it stays
}
