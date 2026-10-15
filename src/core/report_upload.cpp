#include "core/report_upload.hpp"

#include <cstdint>
#include <cstdio>

namespace uagfx::report {
namespace {

void quoted(std::string& out, const std::string& text) {
    out += '"';
    for (const char ch : validUtf8(text)) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += ch;
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else {
            out += ch;
        }
    }
    out += '"';
}

void field(std::string& out, const char* name, const std::string& value) {
    out += '"';
    out += name;
    out += "\":";
    quoted(out, value);
    out += ',';
}

}  // namespace

std::string validUtf8(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        bool ok = n != 0 && i + n <= text.size();
        for (size_t k = 1; ok && k < n; ++k) ok = (static_cast<unsigned char>(text[i + k]) >> 6) == 2;
        if (ok && n == 2) ok = c >= 0xC2;  // no overlong form
        if (ok && n == 3) {
            const unsigned char d = static_cast<unsigned char>(text[i + 1]);
            ok = !(c == 0xE0 && d < 0xA0) && !(c == 0xED && d >= 0xA0);  // overlong, surrogates
        }
        if (ok && n == 4) {
            const unsigned char d = static_cast<unsigned char>(text[i + 1]);
            ok = c <= 0xF4 && !(c == 0xF0 && d < 0x90) && !(c == 0xF4 && d >= 0x90);
        }
        if (!ok) {
            out += '?';
            ++i;
            continue;
        }
        out.append(text, i, n);
        i += n;
    }
    return out;
}

std::string uploadJson(const Upload& u) {
    std::string out = "{";
    field(out, "reason", u.reason);
    out += std::string("\"auto\":") + (u.automatic ? "true" : "false") + ",";
    field(out, "version", u.version);
    field(out, "gpu", u.gpu.substr(0, 200));
    field(out, "windows", u.windows.substr(0, 64));
    field(out, "api", u.api.substr(0, 16));
    field(out, "comment", u.comment.substr(0, 2000));
    field(out, "report", u.report);
    out += "\"files\":[";
    for (size_t i = 0; i < u.files.size(); ++i) {
        if (i) out += ',';
        out += "{\"name\":";
        quoted(out, u.files[i].first);
        out += ",\"text\":";
        quoted(out, u.files[i].second);
        out += '}';
    }
    out += "],\"images\":[";
    size_t sent = 0;
    for (const auto& image : u.images) {
        if (sent == kMostImages || image.second.size() > 140000 || image.second.compare(0, 2, "BM") != 0) continue;
        if (sent++) out += ',';
        out += "{\"name\":";
        quoted(out, image.first);
        out += ",\"data\":\"" + base64(image.second) + "\"}";
    }
    out += "]";
    if (!u.dump.empty() && u.dump.size() <= kMostDump && u.dump.compare(0, 4, "MDMP") == 0)
        out += ",\"dump\":\"" + base64(u.dump) + "\"";
    out += "}";
    return out;
}

std::string base64(const std::string& bytes) {
    static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const uint32_t a = static_cast<unsigned char>(bytes[i]);
        const uint32_t b = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
        const uint32_t c = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
        const uint32_t n = a << 16 | b << 8 | c;
        out += k[n >> 18 & 63];
        out += k[n >> 12 & 63];
        out += i + 1 < bytes.size() ? k[n >> 6 & 63] : '=';
        out += i + 2 < bytes.size() ? k[n & 63] : '=';
    }
    return out;
}

std::string tailOf(const std::string& text, size_t most) {
    if (text.size() <= most) return text;
    size_t from = text.size() - most;
    const size_t line = text.find('\n', from);
    from = line == std::string::npos ? from : line + 1;
    return "(" + std::to_string(from) + " bytes before this left out)\n" + text.substr(from);
}

bool codeOf(const std::string& answer, std::string& code) {
    const std::string key = "\"code\":\"";
    const size_t at = answer.find(key);
    if (at == std::string::npos) return false;
    const size_t start = at + key.size(), end = answer.find('"', start);
    if (end == std::string::npos || end == start || end - start > 16) return false;
    code = answer.substr(start, end - start);
    return true;
}

std::string reasonOfLog(const std::string& log) {
    if (log.find("crash: exception 0x") != std::string::npos) return "crash";  // 0.52: win/crash.cpp
    if (log.find("the uaRO renderer could not start") != std::string::npos) return "start-failed";
    if (log.find("the GPU device was removed") != std::string::npos ||
        log.find("the graphics card was lost") != std::string::npos)
        return "gpu-lost";
    // The game opens DirectDraw again as it changes modes: its last opening must have been closed.
    const size_t opened = log.rfind("(uaRO renderer)");
    if (opened == std::string::npos) return "";
    return log.find("DirectDraw released, renderer closed", opened) == std::string::npos ? "crash" : "";
}

std::string apiOfLog(const std::string& log) {
    const size_t d12 = log.rfind("Direct3D 12 renderer on"), d11 = log.rfind("Direct3D 11 renderer on");
    if (d12 != std::string::npos || d11 != std::string::npos)
        return d11 == std::string::npos || (d12 != std::string::npos && d12 > d11) ? "d3d12" : "d3d11";
    return log.find("DirectDrawCreateEx = ") != std::string::npos ? "dgvoodoo" : "";
}

}  // namespace uagfx::report
