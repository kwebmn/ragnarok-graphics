#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::json {

// 2.3: JSON for uaRO Studio - the lines of its pipe to the engine (core/studio_link.hpp), its session and its queue
// (core/studio_state.hpp). Strings are bytes: UTF-8 as it is, and the game's CP949 file names go through unchanged
// (only '"', '\' and the control characters are escaped).
class Value {
public:
    enum Type : uint8_t { Null, Bool, Number, String, Array, Object };
    Value() = default;
    Value(bool b) : type_(Bool), bool_(b) {}
    Value(int n) : type_(Number), number_(n), whole_(true), int_(n) {}
    Value(int64_t n) : type_(Number), number_(double(n)), whole_(true), int_(n) {}
    Value(double n) : type_(Number), number_(n) {}
    Value(const char* s) : type_(String), string_(s ? s : "") {}
    Value(std::string s) : type_(String), string_(std::move(s)) {}
    static Value array();
    static Value object();

    Type type() const { return type_; }
    bool asBool(bool fallback = false) const { return type_ == Bool ? bool_ : fallback; }
    double asNumber(double fallback = 0) const { return type_ == Number ? number_ : fallback; }
    int64_t asInt(int64_t fallback = 0) const;
    const std::string& asString() const;  // "" when it is no string
    const std::vector<Value>& items() const;  // empty when it is no array
    const std::vector<std::pair<std::string, Value>>& members() const;  // empty when it is no object
    const Value& operator[](const std::string& key) const;  // a null value when there is no such member
    const Value& operator[](size_t i) const;                // a null value past the end
    bool has(const std::string& key) const;
    Value& set(const std::string& key, Value v);  // a member of that name already there gets the value
    Value& push(Value v);                         // onto an array (a null value becomes one)

private:
    friend class Parser;
    friend std::string dump(const Value& v);
    Type type_ = Null;
    bool bool_ = false;
    double number_ = 0;
    bool whole_ = false;  // an integer: written and read without a fraction (ids, times)
    int64_t int_ = 0;
    std::string string_;
    std::vector<Value> items_;
    std::vector<std::pair<std::string, Value>> members_;
};

bool parse(const std::string& text, Value& out, std::string& error);  // at most 64 arrays and objects deep
std::string dump(const Value& v);  // one line, no spaces

}  // namespace uagfx::json
