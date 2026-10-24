#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace uagfx::win::gamefiles {

// The game's files as uaRO.exe finds them (3.0.4-beta.7, from 2.0's map objects, gpu/map_world.cpp): the data folder
// first, then updates\'s GRFs (3.0.2-beta.5, none with [client] updates off), then DATA.INI's archives in the order
// of their numbers - each opened for reading alone when first looked in (2.3.1: the game holds them FILE_SHARE_READ).

bool readWhole(const std::string& path, std::vector<uint8_t>& out);  // a file whole, 64 MB at most; false empty
std::vector<std::string> archives(const std::string& gameDir);       // as the game looks in them

// "data\wav\se_prtbird_01.wav" from the data folder or the archives; `from` - where it was. One thread's.
class Reader {
public:
    explicit Reader(std::string gameDir);
    ~Reader();
    bool read(const std::string& entry, std::vector<uint8_t>& out, std::string* from = nullptr);

private:
    struct Archive;
    std::string dir_;
    std::vector<std::unique_ptr<Archive>> archives_;
    bool listed_ = false;
};

}  // namespace uagfx::win::gamefiles
