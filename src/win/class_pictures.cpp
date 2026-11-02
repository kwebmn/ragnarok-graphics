#include "win/class_pictures.hpp"

#include <windows.h>

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "core/class_art.hpp"
#include "core/jpeg_image.hpp"
#include "win/head_picture.hpp"
#include "win/log.hpp"

namespace uagfx::win::classpictures {
namespace {

HMODULE ours() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ours), &module);
    return module;
}

bool load(int id, panel::Picture& out) {
    const HMODULE module = ours();
    const HRSRC found = module ? FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10)) : nullptr;  // RCDATA
    const HGLOBAL loaded = found ? LoadResource(module, found) : nullptr;
    const void* data = loaded ? LockResource(loaded) : nullptr;
    const DWORD size = found ? SizeofResource(module, found) : 0;
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    if (!data || !size || !jpeg::decode(static_cast<const uint8_t*>(data), size, argb, w, h)) return false;
    out.width = w;
    out.height = h;
    out.px = std::move(argb);
    return true;
}

}  // namespace

const panel::Picture* forJob(int jobId) {
    static std::map<std::string, panel::Picture> made;  // by the portrait's file; an empty one: not readable
    const char* file = classart::pictureFor(jobId);
    if (!file) return nullptr;
    auto it = made.find(file);
    if (it == made.end()) {
        panel::Picture p;
        if (!load(classart::resourceOf(file), p)) LOG_INFO("login: the portrait %s could not be read", file);
        it = made.emplace(file, std::move(p)).first;
    }
    return it->second.width > 0 ? &it->second : nullptr;
}

const panel::Picture* head(int sex, int hair, int colour, int side) {
    static std::map<std::tuple<int, int, int, int>, panel::Picture> made;
    const auto key = std::make_tuple(sex, hair, colour, side);
    auto it = made.find(key);
    if (it == made.end()) {
        panel::Picture p;
        headpicture::make(sex, hair, colour, side, p);
        it = made.emplace(key, std::move(p)).first;
    }
    return it->second.width > 0 ? &it->second : nullptr;
}

}  // namespace uagfx::win::classpictures
