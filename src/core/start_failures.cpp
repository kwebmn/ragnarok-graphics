#include "core/start_failures.hpp"

#include <sstream>

namespace uagfx::startfail {
namespace {

int countOf(const std::string& text, const std::string& version) {
    std::istringstream in(text);
    std::string v;
    int n = 0;
    if (!(in >> v >> n) || v != version || n < 0) return 0;
    return n;
}

}  // namespace

bool failed(const std::string& reason, int minutes) {
    return reason == "start-failed" || (reason == "crash" && minutes >= 0 && minutes < kEarlyMinutes);
}

std::string note(const std::string& text, const std::string& version, bool failedNow) {
    if (!failedNow) return "";
    return version + " " + std::to_string(countOf(text, version) + 1) + "\n";
}

bool offerRollback(const std::string& text, const std::string& version) {
    return !version.empty() && countOf(text, version) >= kOffer;
}

}  // namespace uagfx::startfail
