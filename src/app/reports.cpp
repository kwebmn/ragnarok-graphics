#include "app/reports.hpp"

#include <atomic>
#include <ctime>
#include <sstream>

#include "app/download.hpp"
#include "app/files.hpp"
#include "app/system.hpp"
#include "app/text.hpp"
#include "core/json.hpp"
#include "core/layout.hpp"
#include "core/report_answers.hpp"

namespace uagfx::app {
namespace {

constexpr const char* kPath = "/api/graphics/reports";
constexpr size_t kMostAnswer = 1u << 20;  // a long conversation's JSON
constexpr size_t kMostShot = 40'000'000;  // a developer's PNG; 3.0.5-beta.5 a shot of the player's in full (shotPng)

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

enum class Outcome { Sent, Keep, Limited, Refused };  // 3.0.22-beta.17: Limited - kept, the site's hour full

// One report's JSON to the site: its code when it went
Outcome postReport(const std::string& body, std::string& code, std::string& error) {
    long status = 0;
    std::string answer;
    if (!postPath(kPath, body, status, answer, error)) return Outcome::Keep;
    if (status == 201 && report::codeOf(answer, code)) return Outcome::Sent;
    error = status == 429 ? "too many reports from this computer - it goes later by itself"
                          : "uaro.kiev.ua answered " + std::to_string(status);
    if (status == 429) return Outcome::Limited;
    return status >= 500 ? Outcome::Keep : Outcome::Refused;
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
    noteSentIn(dir, code, v["reason"].asString().empty() ? "manual" : v["reason"].asString(), v["auto"].asBool(),
               v["version"].asString());
}

}  // namespace

std::string existingComputerKey(const std::wstring& dir) {
    std::string key;
    if (!readFile(join(dir, wide(layout::kComputerKey)), key)) return "";
    while (!key.empty() && (key.back() == '\n' || key.back() == '\r' || key.back() == ' ')) key.pop_back();
    return validKey(key) ? key : "";
}

std::string computerKey(const std::wstring& dir) {
    std::string key = existingComputerKey(dir);
    if (!key.empty()) return key;
    key = randomKey();
    makeReportsFolder(dir);
    writeFileAtomic(join(dir, wide(layout::kComputerKey)), key);
    return key;
}

void noteAnswersOf(const std::wstring& dir, const std::vector<reports::Summary>& list) {
    const std::wstring file = join(dir, wide(layout::kAnswers));
    const std::string text = answers::write(reports::answersOf(list));
    std::string was;
    if (readFile(file, was) && was == text) return;  // the same: its time stays (the driver reads it by its time)
    makeReportsFolder(dir);
    writeFileAtomic(file, text);
}

bool noteAnswers(const std::wstring& dir) {
    const std::string key = existingComputerKey(dir);
    if (key.empty()) return false;
    std::vector<reports::Summary> list;
    std::string error;
    if (!listReports(key, list, error)) return false;
    noteAnswersOf(dir, list);
    return true;
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

bool sendStillThere(const std::string& key, const std::string& code, const std::string& text,
                    const std::string& stillThere, const std::vector<std::pair<std::string, std::string>>& shots,
                    std::string& error) {
    if (shots.empty()) return sendVerdict(key, code, false, text, error);
    return answerReport(key, code, text.empty() ? stillThere : text, shots, error);
}

bool sendOriginal(const std::string& key, const std::string& code, const std::string& name, const std::string& png,
                  std::string& error, const std::function<void(size_t, size_t)>& sent) {
    constexpr size_t kPart = 1u << 20;  // the site takes a request of 8 MB at most: a part's base64 under 1.4 MB
    const size_t parts = (png.size() + kPart - 1) / kPart;
    if (!validCode(code) || png.empty() || parts > 48) {
        error = "not a shot to send: " + name;
        return false;
    }
    for (size_t i = 0; i < parts; ++i) {
        json::Value body = json::Value::object();
        body.set("computer", key);
        body.set("name", name);
        body.set("part", int(i));
        body.set("parts", int(parts));
        body.set("at", int64_t(i * kPart));  // what the site has of it so far: the parts go in their order
        body.set("data", report::base64(png.substr(i * kPart, kPart)));
        if (!post(reportPath(code) + "/originals", body, 201, error)) return false;
        if (sent) sent(i + 1, parts);  // 3.0.20-beta.17
    }
    return true;
}

bool fetchShot(const std::string& key, const std::string& code, const std::string& name, std::string& bytes,
               std::string& error, bool full) {
    if (!validCode(code) || name.find_first_of("/\\?&#%") != std::string::npos) {
        error = "not a shot of a report: " + name;
        return false;
    }
    return fetchPath(reportPath(code) + "/shots/" + name + "?computer=" + key + (full ? "&full=1" : ""), bytes, error,
                     kMostShot);
}

NewReport sendNew(const std::wstring& dir, const report::Upload& u) {
    NewReport r;
    const std::string body = report::uploadJson(u);
    const Outcome o = postReport(body, r.code, r.error);
    switch (o) {
        case Outcome::Sent:
            noteSentIn(dir, r.code, u.reason, u.automatic, u.version);
            sendQueued(dir);  // the site answers: the waiting ones go too
            break;
        case Outcome::Keep:
        case Outcome::Limited:
            r.limited = o == Outcome::Limited;
            r.queued = queue(dir, body);
            if (!r.queued) r.error += " (and it could not be kept for later)";
            break;
        case Outcome::Refused:
            break;
    }
    return r;
}

int sendQueued(const std::wstring& dir) {
    // 3.0.20-beta.17: «Звернення»'s list and a new report go on threads of their own now - one sends the waiting ones,
    // another meanwhile leaves them to it
    static std::atomic<bool> busy{false};
    if (busy.exchange(true)) return 0;
    struct Free {
        ~Free() { busy = false; }
    } lettingGo;
    int sent = 0;
    for (const std::wstring& file : queuedFiles(dir)) {
        std::string body, code, error;
        if (!readFile(file, body)) continue;
        const Outcome o = postReport(body, code, error);
        if (o == Outcome::Keep || o == Outcome::Limited) break;  // the site does not take it now: the rest wait too
        if (o == Outcome::Sent) {
            noteQueued(dir, body, code);
            ++sent;
        }
        deleteFile(file);  // sent, or refused for good
    }
    return sent;
}

int queuedReports(const std::wstring& dir) { return int(queuedFiles(dir).size()); }

void noteSentIn(const std::wstring& dir, const std::string& code, const std::string& reason, bool automatic,
                const std::string& version) {
    deleteFile(join(dir, L"uaro-graphics\\crash.dmp"));  // 0.52: on the site now
    std::string text;
    readFile(sentFile(dir), text);
    text += reports::sentLine(static_cast<long long>(std::time(nullptr)), code, reason, automatic, version);
    makeFolder(join(dir, wide(layout::kOurs)));
    writeFileAtomic(sentFile(dir), report::tailOf(text, 16u << 10));
}

long long lastBadEndIn(const std::wstring& dir, const std::string& version) {
    std::string text;
    readFile(sentFile(dir), text);
    return reports::lastBadEnd(text, version);
}

}  // namespace uagfx::app
