#include "core/updates.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::updates {

std::string lookupKey(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        const auto c = static_cast<unsigned char>(name[i]);
        if (c >= 0x81 && c <= 0xFE && i + 1 < name.size()) {  // a CP949 character: both bytes as they are
            out += name[i];
            out += name[++i];
        } else {
            out += c == '/' ? '\\' : (c >= 'A' && c <= 'Z') ? char(c + 32) : char(c);
        }
    }
    return out;
}

std::vector<std::string> lookupOrder(std::vector<std::string> files) {
    const auto lower = [](std::string s) {
        for (char& c : s) c = (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
        return s;
    };
    files.erase(std::remove_if(files.begin(), files.end(),
                               [&](const std::string& f) {
                                   const std::string l = lower(f);
                                   return l.size() <= 4 || l.compare(l.size() - 4, 4, ".grf") != 0;
                               }),
                files.end());
    std::sort(files.begin(), files.end(),
              [&](const std::string& a, const std::string& b) { return lower(a) > lower(b); });
    return files;
}

std::vector<std::string> archiveOrder(const std::vector<std::string>& files, const std::vector<std::string>& dataIni) {
    std::vector<std::string> out;
    for (const std::string& f : lookupOrder(files)) out.push_back(std::string(kFolder) + "\\" + f);
    out.insert(out.end(), dataIni.begin(), dataIni.end());
    return out;
}

void Index::add(int archive, const std::vector<std::string>& names) {
    for (const std::string& n : names) hits_.emplace(lookupKey(n), Hit{archive, n});
}

const Hit* Index::find(const std::string& name) const {
    const auto it = hits_.find(lookupKey(name));
    return it == hits_.end() ? nullptr : &it->second;
}

HookPlan hookPlan(bool getDataKnown, bool updatesOn, size_t files, bool lookupsKnown) {
    HookPlan p;
    p.inTurn = getDataKnown;
    p.updates = getDataKnown && updatesOn && files > 0 && lookupsKnown;
    return p;
}

bool tellWait(unsigned n) {
    constexpr unsigned kFirst = 5, kEvery = 100;
    return n > 0 && (n <= kFirst || n % kEvery == 0);
}

namespace {

// `s` begins with `head` (its ASCII letters in either case)
bool beginsWith(const std::string& s, const char* head) {
    const size_t n = std::strlen(head);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        const char a = s[i] >= 'A' && s[i] <= 'Z' ? char(s[i] - 'A' + 'a') : s[i] == '/' ? '\\' : s[i];
        if (a != head[i]) return false;
    }
    return true;
}

}  // namespace

std::string paletteSprite(const std::string& name) {
    static const std::string kBody = "\xB8\xF6", kHair = "\xB8\xD3\xB8\xAE";  // 몸, 머리
    static const std::string kMale = "\xB3\xB2", kFemale = "\xBF\xA9";          // 남, 여
    static const std::string kHuman = "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\";  // data\sprite\인간족\.
    if (!beginsWith(name, "data\\palette\\") || name.size() < 18) return "";
    const std::string rest = name.substr(13);
    const size_t slash = rest.find_first_of("\\/");
    if (slash == std::string::npos) return "";
    const std::string folder = rest.substr(0, slash);
    std::string file = rest.substr(slash + 1);
    if (file.size() < 5 || !beginsWith(file.substr(file.size() - 4), ".pal")) return "";
    file.resize(file.size() - 4);
    const size_t under = file.rfind('_');
    if (under == std::string::npos || under + 1 >= file.size() ||
        file.find_first_not_of("0123456789", under + 1) != std::string::npos)
        return "";
    const std::string base = file.substr(0, under);  // <job>_<sex>, 머리<style>_<sex>
    std::string sex;
    for (const std::string* s : {&kMale, &kFemale})
        if (base.size() > s->size() + 1 && base.compare(base.size() - s->size() - 1, s->size() + 1, "_" + *s) == 0)
            sex = *s;
    if (sex.empty()) return "";
    const std::string stem = base.substr(0, base.size() - sex.size() - 1);
    if (folder == kBody) return kHuman + "\xB8\xF6\xC5\xEB\\" + sex + "\\" + stem + "_" + sex + ".spr";  // 몸통
    if (folder == kHair && stem.size() > kHair.size() && stem.compare(0, kHair.size(), kHair) == 0)
        return kHuman + "\xB8\xD3\xB8\xAE\xC5\xEB\\" + sex + "\\" + stem.substr(kHair.size()) + "_" + sex +
               ".spr";  // 머리통
    return "";
}

bool spritePalette(const std::vector<uint8_t>& spr, std::vector<uint8_t>& palette) {
    if (spr.size() < 6 + 1024 || spr[0] != 'S' || spr[1] != 'P') return false;
    const unsigned version = unsigned(spr[3]) << 8 | spr[2];  // 2.1: 0x01 0x02
    if (version < 0x101) return false;
    palette.assign(spr.end() - 1024, spr.end());
    return true;
}

}  // namespace uagfx::updates
