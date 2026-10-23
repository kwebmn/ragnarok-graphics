#include "core/client_libs.hpp"

namespace uagfx::client_libs {

const std::vector<Lib>& all() {
    static const std::vector<Lib> libs = {
        {"cps.dll", "cps"},       // zlib's compress and uncompress (the GRF archives' files)
        {"binkw32.dll", "bink"},  // RAD's Bink video: none in uaRO
        {"ijl15.dll", "ijl"},     // 3.0.1-beta.6: Intel JPEG Library 1.5 (the loading pictures, Print Screen)
    };
    return libs;
}

bool ours(const std::string& bytes) { return bytes.find("uaRO Graphics client library") != std::string::npos; }

Step decide(bool wanted, bool haveOurs, Root root, bool kept) {
    if (wanted && haveOurs) return root == Root::Ours ? Step::Nothing : Step::PutOurs;
    const bool ourOrNone = root == Root::Ours || root == Root::OursOther || root == Root::Missing;
    return ourOrNone && kept ? Step::RestoreOriginal : Step::Nothing;
}

}  // namespace uagfx::client_libs
