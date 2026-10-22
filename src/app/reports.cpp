#include "app/reports.hpp"

#include <ctime>
#include <sstream>

#include "app/download.hpp"
#include "app/files.hpp"
#include "app/system.hpp"
#include "app/text.hpp"
#include "core/json.hpp"
#include "core/layout.hpp"

namespace uagfx::app {
namespace {

constexpr const char* kPath = "/api/graphics/reports";
constexpr size_t kMostAnswer = 1u << 20;  // a long conversation's JSON
constexpr size_t kMostShot = 8u << 20;    // a developer's PNG

std::wstring join(const std::wstring& a, const std::wstring& b) { return a + L"\\" + b; }
std::wstring sentFile(const std::wstring& dir) { return join(dir, L"uaro-graphics\\reports.txt"); }

bool validKey(const std::string& key) {
    return key.size() >= 16 && key.size() <= 32 && key.find_first_not_of("0123456789abcdef") == std::string::npos;
}

// The code goes into a path: "R-" and six of the site's letters and digits
bool validCode(const std::string& code) {
    if (code.size() != 8 || code.compare(0, 2, "R-") != 0) return false;
    for (size_t i = 2; i < code.size(); ++i)
        if (!((code[i] >= 'A' && code[i] <= 'Z') || (code[i] >= '0' && code[i] <= '9'))) return false;
    return true;
}

std::string reportPath(const std::string& code) { return std::string(kPath) + "/" + code; }

// A POST of the player's: true on `expected`, else false and why
bool post(const std::string& path, const json::Value& body, long expected, std::string& error) {
    long status = 0;
    std::string answer;
    if (!postPath(path, json::dump(body), status, answer, error)) return false;
    if (status != expected) {
        error = "uaro.kiev.ua answered " + std::to_string(status);
        return false;
    }
    return true;
}

struct Sent {
    long long time = 0;
    std::string code, reason;
    bool automatic = false;
};

std::vector<Sent> readSent(const std::wstring& dir) {  // oldest first
    std::vector<Sent> out;
    std::string text;
    if (!readFile(sentFile(dir), text)) return out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        Sent s;
        std::string kind;
        if (l >> s.time >> s.code >> s.reason >> kind) {
            s.automatic = kind == "auto";
            out.push_back(s);
        }
    }
    return out;
}

enum class Outcome { Sent, Keep, Refused };

// One report's JSON to the site: its code when it went
Outcome postReport(const std::string& body, std::string& code, std::string& error) {
    long status = 0;
    std::string answer;
    if (!postPath(kPath, body, status, answer, error)) return Outcome::Keep;
    if (status == 201 && report::codeOf(answer, code)) return Outcome::Sent;
    error = status == 429 ? "too many reports from this computer - it goes later by itself"
                          : "uaro.kiev.ua answered " + std::to_string(status);
    return status == 429 || status >= 500 ? Outcome::Keep : Outcome::Refused;
}

std::vector<std::wstring> queuedFiles(const std::wstring& dir) {
    std::vector<std::wstring> out;
    for (const std::wstring& name : listFolder(join(dir, wide(layout::kReports)), false))
        if (name.rfind(L"queued-", 0) == 0 && name.size() > 5 && name.compare(name.size() - 5, 5, L".json") == 0)
            out.push_back(join(join(dir, wide(layout::kReports)), name));
    return out;
}

bool makeReportsFolder(const std::wstring& dir) {
    makeFolder(join(dir, wide(layout::kOurs)));
    return makeFolder(join(dir, wide(layout::kReports)));
}

bool queue(const std::wstring& dir, const std::string& body) {
    if (!makeReportsFolder(dir)) return false;
    const std::wstring base = join(join(dir, wide(layout::kReports)), L"queued-" + timestampName());
    std::wstring path = base + L".json";
    for (int n = 2; fileExists(path); ++n) path = base + L"-" + std::to_wstring(n) + L".json";
    return writeFileAtomic(path, body);
}

// What the site needs to note a queued report as sent: its reason and whether it went by itself
void noteQueued(const std::wstring& dir, const std::string& body, const std::string& code) {
    json::Value v;
    std::string ignored;
    json::parse(body, v, ignored);
    noteSentIn(dir, code, v["reason"].asString().empty() ? "manual" : v["reason"].asString(), v["auto"].asBool());
}

}  // namespace

std::string computerKey(const std::wstring& dir) {
    const std::wstring file = join(dir, wide(layout::kComputerKey));
    std::string key;
    if (readFile(file, key)) {
        while (!key.empty() && (key.back() == '\n' || key.back() == '\r' || key.back() == ' ')) key.pop_back();
        if (validKey(key)) return key;
    }
    key = randomKey();
    makeReportsFolder(dir);
    writeFileAtomic(file, key);
    return key;
}

bool listReports(const std::string& key, std::vector<reports::Summary>& out, std::string& error) {
    std::string text;
    if (!fetchPath(std::string(kPath) + "/mine?computer=" + key, text, error, kMostAnswer)) return false;
    return reports::parseList(text, out, error);
}

bool openReport(const std::string& key, const std::string& code, reports::Conversation& out, std::string& error) {
    if (!validCode(code)) {
        error = "not a report's code: " + code;
        return false;
    }
    std::string text;
    if (!fetchPath(reportPath(code) + "?computer=" + key, text, error, kMostAnswer)) return false;
    return reports::parseConversation(text, out, error);
}

bool answerReport(const std::string& key, const std::string& code, const std::string& text,
                  const std::vector<std::pair<std::string, std::string>>& shots, std::string& error) {
    if (!validCode(code)) {
        error = "not a report's code: " + code;
        return false;
    }
    json::Value body = json::Value::object();
    body.set("computer", key);
    body.set("text", report::validUtf8(text));
    json::Value list = json::Value::array();
    for (const auto& shot : shots) {
        json::Value one = json::Value::object();
        one.set("name", shot.first);
        one.set("data", report::base64(shot.second));
        list.push(one);
    }
    if (!shots.empty()) body.set("screenshots", list);
    return post(reportPath(code) + "/messages", body, 201, error);
}

bool sendVerdict(const std::string& key, const std::string& code, bool fixed, const std::string& text,
                 std::string& error) {
    if (!validCode(code)) {
        error = "not a report's code: " + code;
        return false;
    }
    json::Value body = json::Value::object();
    body.set("computer", key);
    body.set("fixed", fixed);
    if (!text.empty()) body.set("text", report::validUtf8(text));
    return post(reportPath(code) + "/verdict", body, 200, error);
}

bool fetchShot(const std::string& key, const std::string& code, const std::string& name, std::string& bytes,
               std::string& error) {
    if (!validCode(code) || name.find_first_of("/\\?&#%") != std::string::npos) {
        error = "not a shot of a report: " + name;
        return false;
    }
    return fetchPath(reportPath(code) + "/shots/" + name + "?computer=" + key, bytes, error, kMostShot);
}

NewReport sendNew(const std::wstring& dir, const report::Upload& u) {
    NewReport r;
    const std::string body = report::uploadJson(u);
    switch (postReport(body, r.code, r.error)) {
        case Outcome::Sent:
            noteSentIn(dir, r.code, u.reason, u.automatic);
            sendQueued(dir);  // the site answers: the waiting ones go too
            break;
        case Outcome::Keep:
            r.queued = queue(dir, body);
            if (!r.queued) r.error += " (and it could not be kept for later)";
            break;
        case Outcome::Refused:
            break;
    }
    return r;
}

int sendQueued(const std::wstring& dir) {
    int sent = 0;
    for (const std::wstring& file : queuedFiles(dir)) {
        std::string body, code, error;
        if (!readFile(file, body)) continue;
        const Outcome o = postReport(body, code, error);
        if (o == Outcome::Keep) break;  // the site does not answer: the rest wait too
        if (o == Outcome::Sent) {
            noteQueued(dir, body, code);
            ++sent;
        }
        deleteFile(file);  // sent, or refused for good
    }
    return sent;
}

int queuedReports(const std::wstring& dir) { return int(queuedFiles(dir).size()); }

void noteSentIn(const std::wstring& dir, const std::string& code, const std::string& reason, bool automatic) {
    deleteFile(join(dir, L"uaro-graphics\\crash.dmp"));  // 0.52: on the site now
    std::string text;
    readFile(sentFile(dir), text);
    text += std::to_string(static_cast<long long>(std::time(nullptr))) + " " + code + " " + reason + " " +
            (automatic ? "auto" : "manual") + "\n";
    makeFolder(join(dir, wide(layout::kOurs)));
    writeFileAtomic(sentFile(dir), report::tailOf(text, 16u << 10));
}

std::vector<std::string> sentCodesIn(const std::wstring& dir, size_t most) {
    const std::vector<Sent> all = readSent(dir);
    std::vector<std::string> out;
    for (auto it = all.rbegin(); it != all.rend() && out.size() < most; ++it) out.push_back(it->code);
    return out;
}

long long lastAutomaticIn(const std::wstring& dir) {
    long long last = 0;
    for (const Sent& s : readSent(dir))
        if (s.automatic && s.time > last) last = s.time;
    return last;
}

}  // namespace uagfx::app
