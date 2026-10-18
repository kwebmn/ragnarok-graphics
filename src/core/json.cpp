#include "core/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace uagfx::json {
namespace {

const Value& nullValue() {
    static const Value none;
    return none;
}

void putUtf8(std::string& out, uint32_t c) {
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xC0 | (c >> 6));
        out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += char(0xE0 | (c >> 12));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    } else {
        out += char(0xF0 | (c >> 18));
        out += char(0x80 | ((c >> 12) & 0x3F));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    }
}

void dumpString(const std::string& s, std::string& out) {
    out += '"';
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) {
            char u[8];
            std::snprintf(u, sizeof u, "\\u%04x", c);
            out += u;
        } else {
            out += ch;
        }
    }
    out += '"';
}

void dumpTo(const Value& v, std::string& out);

}  // namespace

class Parser {
public:
    explicit Parser(const std::string& text) : p_(text.data()), end_(text.data() + text.size()) {}
    bool document(Value& out, std::string& error) {
        if (!value(out, 0)) {
            error = error_.empty() ? "not JSON" : error_;
            return false;
        }
        space();
        if (p_ != end_) {
            error = "more after the value";
            return false;
        }
        return true;
    }

private:
    bool fail(const char* why) {
        if (error_.empty()) error_ = why;
        return false;
    }
    void space() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_;
    }
    bool word(const char* w) {
        const size_t n = std::strlen(w);
        if (size_t(end_ - p_) < n || std::memcmp(p_, w, n) != 0) return fail("an unknown word");
        p_ += n;
        return true;
    }
    bool hex4(uint32_t& c) {
        if (end_ - p_ < 4) return fail("a short \\u");
        c = 0;
        for (int i = 0; i < 4; ++i, ++p_) {
            const char h = *p_;
            c <<= 4;
            if (h >= '0' && h <= '9') c |= uint32_t(h - '0');
            else if (h >= 'a' && h <= 'f') c |= uint32_t(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') c |= uint32_t(h - 'A' + 10);
            else return fail("a bad \\u");
        }
        return true;
    }
    bool string(std::string& out) {
        ++p_;  // the opening quote
        while (p_ < end_ && *p_ != '"') {
            if (static_cast<unsigned char>(*p_) < 0x20) return fail("a control character in a string");
            if (*p_ != '\\') {
                out += *p_++;
                continue;
            }
            if (++p_ == end_) break;
            const char e = *p_++;
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                uint32_t c = 0;
                if (!hex4(c)) return false;
                if (c >= 0xDC00 && c <= 0xDFFF) return fail("half a surrogate pair");
                if (c >= 0xD800 && c <= 0xDBFF) {  // a pair: the low half must follow
                    uint32_t low = 0;
                    if (end_ - p_ < 2 || p_[0] != '\\' || p_[1] != 'u') return fail("half a surrogate pair");
                    p_ += 2;
                    if (!hex4(low)) return false;
                    if (low < 0xDC00 || low > 0xDFFF) return fail("half a surrogate pair");
                    c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                }
                putUtf8(out, c);
                break;
            }
            default: return fail("an unknown escape");
            }
        }
        if (p_ == end_) return fail("a string not closed");
        ++p_;
        return true;
    }
    bool number(Value& out) {
        const char* start = p_;
        if (p_ < end_ && *p_ == '-') ++p_;
        bool digits = false, whole = true;
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_, digits = true;
        if (p_ < end_ && *p_ == '.') {
            whole = false;
            ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            whole = false;
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        if (!digits) return fail("a bad number");
        const std::string text(start, p_);
        out = Value(std::strtod(text.c_str(), nullptr));
        if (whole && text.size() < 19) {  // fits an int64 with room
            out.whole_ = true;
            out.int_ = std::strtoll(text.c_str(), nullptr, 10);
        }
        return true;
    }
    bool value(Value& out, int depth) {
        space();
        if (p_ == end_) return fail("no value");
        const char c = *p_;
        if (c == '{' || c == '[') {
            if (depth >= 64) return fail("too deep");
            const bool object = c == '{';
            out = object ? Value::object() : Value::array();
            ++p_;
            space();
            if (p_ < end_ && *p_ == (object ? '}' : ']')) {
                ++p_;
                return true;
            }
            for (;;) {
                space();
                std::string key;
                if (object) {
                    if (p_ == end_ || *p_ != '"') return fail("a member without a name");
                    if (!string(key)) return false;
                    space();
                    if (p_ == end_ || *p_ != ':') return fail("no ':' after a name");
                    ++p_;
                }
                Value item;
                if (!value(item, depth + 1)) return false;
                if (object) out.set(key, std::move(item));
                else out.items_.push_back(std::move(item));
                space();
                if (p_ == end_) return fail("not closed");
                if (*p_ == ',') {
                    ++p_;
                    continue;
                }
                if (*p_ != (object ? '}' : ']')) return fail("no ',' between values");
                ++p_;
                return true;
            }
        }
        if (c == '"') {
            std::string s;
            if (!string(s)) return false;
            out = Value(std::move(s));
            return true;
        }
        if (c == 't') return word("true") && (out = Value(true), true);
        if (c == 'f') return word("false") && (out = Value(false), true);
        if (c == 'n') return word("null") && (out = Value(), true);
        return number(out);
    }

    const char* p_;
    const char* end_;
    std::string error_;
};

namespace {

void dumpTo(const Value& v, std::string& out) {
    switch (v.type()) {
    case Value::Null: out += "null"; break;
    case Value::Bool: out += v.asBool() ? "true" : "false"; break;
    case Value::Number: {
        const double n = v.asNumber();
        char text[32];
        if (std::isfinite(n) && n == std::floor(n) && std::fabs(n) < 9.0e15) {
            std::snprintf(text, sizeof text, "%lld", static_cast<long long>(v.asInt()));
        } else if (std::isfinite(n)) {
            std::snprintf(text, sizeof text, "%.17g", n);
            // the shortest form that reads back the same
            for (int digits = 1; digits <= 17; ++digits) {
                char shorter[32];
                std::snprintf(shorter, sizeof shorter, "%.*g", digits, n);
                if (std::strtod(shorter, nullptr) == n) {
                    std::memcpy(text, shorter, sizeof shorter);
                    break;
                }
            }
        } else {
            std::snprintf(text, sizeof text, "null");  // JSON has no infinity
        }
        out += text;
        break;
    }
    case Value::String: dumpString(v.asString(), out); break;
    case Value::Array: {
        out += '[';
        bool first = true;
        for (const Value& item : v.items()) {
            if (!first) out += ',';
            first = false;
            dumpTo(item, out);
        }
        out += ']';
        break;
    }
    case Value::Object: {
        out += '{';
        bool first = true;
        for (const auto& m : v.members()) {
            if (!first) out += ',';
            first = false;
            dumpString(m.first, out);
            out += ':';
            dumpTo(m.second, out);
        }
        out += '}';
        break;
    }
    }
}

}  // namespace

Value Value::array() {
    Value v;
    v.type_ = Array;
    return v;
}

Value Value::object() {
    Value v;
    v.type_ = Object;
    return v;
}

int64_t Value::asInt(int64_t fallback) const {
    if (type_ != Number) return fallback;
    if (whole_) return int_;
    return std::isfinite(number_) ? static_cast<int64_t>(number_) : fallback;
}

const std::string& Value::asString() const {
    static const std::string none;
    return type_ == String ? string_ : none;
}

const std::vector<Value>& Value::items() const {
    static const std::vector<Value> none;
    return type_ == Array ? items_ : none;
}

const std::vector<std::pair<std::string, Value>>& Value::members() const {
    static const std::vector<std::pair<std::string, Value>> none;
    return type_ == Object ? members_ : none;
}

const Value& Value::operator[](const std::string& key) const {
    for (const auto& m : members())
        if (m.first == key) return m.second;
    return nullValue();
}

const Value& Value::operator[](size_t i) const { return i < items().size() ? items_[i] : nullValue(); }

bool Value::has(const std::string& key) const {
    for (const auto& m : members())
        if (m.first == key) return true;
    return false;
}

Value& Value::set(const std::string& key, Value v) {
    if (type_ != Object) {
        *this = object();
    }
    for (auto& m : members_)
        if (m.first == key) {
            m.second = std::move(v);
            return *this;
        }
    members_.emplace_back(key, std::move(v));
    return *this;
}

Value& Value::push(Value v) {
    if (type_ != Array) *this = array();
    items_.push_back(std::move(v));
    return *this;
}

bool parse(const std::string& text, Value& out, std::string& error) {
    Value v;
    Parser p(text);
    if (!p.document(v, error)) return false;
    out = std::move(v);
    return true;
}

std::string dump(const Value& v) {
    std::string out;
    dumpTo(v, out);
    return out;
}

}  // namespace uagfx::json
