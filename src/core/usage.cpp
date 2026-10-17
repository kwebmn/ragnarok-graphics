#include "core/usage.hpp"

#include <algorithm>
#include <cstdio>

#include "core/effect_groups.hpp"

namespace uagfx::usage {

std::string jsonString(const std::string& text) {
    std::string out = "\"";
    for (const char ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += ch;
        } else if (c < 0x20) {
            char e[8];
            std::snprintf(e, sizeof e, "\\u%04x", c);
            out += e;
        } else {
            out += ch;
        }
    }
    return out + "\"";
}

std::string uploadBody(const std::string& id, const std::string& version, const std::string& lines, size_t most,
                       size_t& taken) {
    taken = 0;
    for (size_t at = lines.find('\n'); at != std::string::npos && at + 1 <= most; at = lines.find('\n', at + 1))
        taken = at + 1;
    return "{\"id\":" + jsonString(id) + ",\"version\":" + jsonString(version) +
           ",\"lines\":" + jsonString(lines.substr(0, taken)) + "}";
}

std::vector<std::string> changes(const EffectSettings& was, const EffectSettings& now, int card,
                                 const std::vector<std::string>& applied) {
    std::vector<std::string> out;
    for (const std::string& line : applied) {
        const std::string key = line.substr(0, line.find(' '));
        if (!effect_groups::memberOf(key) && key != "tier") out.push_back(line);
    }
    if (now.preset != EffectPreset::Custom) return out;
    const EffectSettings a = effect_groups::shown(was, card), b = effect_groups::shown(now, card);
    for (int i = 0; i < effect_groups::memberCount(); ++i) {
        const effect_groups::Member& m = effect_groups::members()[i];
        if (a.*m.on != b.*m.on) out.push_back(std::string(m.key) + (a.*m.on ? " 1 -> 0" : " 0 -> 1"));
    }
    static const char* const kTiers[] = {"auto", "classic", "remaster"};
    if (a.tier != b.tier)
        out.push_back(std::string("tier ") + kTiers[std::clamp(a.tier, 0, 2)] + " -> " +
                      kTiers[std::clamp(b.tier, 0, 2)]);
    return out;
}

void Recorder::begin(const Machine& m, uint64_t session) {
    machine_ = m;
    char id[24];
    std::snprintf(id, sizeof id, "%016llx", static_cast<unsigned long long>(session));
    session_ = id;
    begun_ = true;
    open_ = false;
    total_ = 0.0;
}

void Recorder::frame(double seconds, bool outdoors, double gpuMs, const Setup& s, std::string& out) {
    if (!begun_ || seconds <= 0.0 || seconds > 1.0) return;  // a stall (loading, dragged window) counts nothing
    if (open_ && (s != setup_ || minutes_ >= kStretchMinutes)) line(out);
    if (!open_) {
        setup_ = s;
        open_ = true;
        minutes_ = outdoor_ = frames_ = gpuSum_ = 0.0;
        gpuCount_ = 0;
    }
    minutes_ += seconds / 60.0;
    total_ += seconds / 60.0;
    if (outdoors) outdoor_ += seconds / 60.0;
    frames_ += 1.0;
    if (gpuMs >= 0.0) {
        gpuSum_ += gpuMs;
        ++gpuCount_;
    }
}

void Recorder::change(const std::string& what, std::string& out) {
    if (!begun_) return;
    char at[32];
    std::snprintf(at, sizeof at, "%.2f", total_);
    out += "{\"t\":\"c\",\"sid\":" + jsonString(session_) + ",\"at\":" + at + ",\"what\":" + jsonString(what) + "}\n";
}

void Recorder::close(std::string& out) {
    if (open_) line(out);
}

void Recorder::line(std::string& out) {
    open_ = false;
    if (minutes_ < 0.05) return;  // a few seconds: nothing to learn from
    char numbers[160];
    std::snprintf(numbers, sizeof numbers, "\"min\":%.2f,\"outdoor\":%.2f,\"fps\":%.1f,\"gpu\":%.2f", minutes_,
                  outdoor_, minutes_ > 0.0 ? frames_ / (minutes_ * 60.0) : 0.0,
                  gpuCount_ ? gpuSum_ / gpuCount_ : -1.0);
    char machine[96];
    std::snprintf(machine, sizeof machine, "\"vendor\":%u,\"vram\":%llu,\"level\":%d,\"ai\":%d", machine_.vendor,
                  machine_.vramMb, machine_.level, setup_.aiKinds);
    out += "{\"t\":\"s\",\"sid\":" + jsonString(session_) + ",\"v\":" + jsonString(machine_.version) +
           ",\"card\":" + jsonString(machine_.card) + ",\"api\":" + jsonString(machine_.api) + "," + machine +
           ",\"game\":" + jsonString(setup_.game) + ",\"out\":" + jsonString(setup_.output) +
           ",\"dlss\":" + jsonString(setup_.dlss) + ",\"preset\":" + jsonString(setup_.preset) +
           ",\"groups\":" + jsonString(setup_.groups) + ",\"on\":" + jsonString(setup_.effects) +
           ",\"aimodel\":" + jsonString(setup_.aiModel) + "," + numbers + "}\n";
}

}  // namespace uagfx::usage
