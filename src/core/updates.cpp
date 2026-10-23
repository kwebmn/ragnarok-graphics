#include "core/updates.hpp"

#include <algorithm>

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

}  // namespace uagfx::updates
