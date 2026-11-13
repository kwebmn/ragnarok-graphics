// 3.0.18-beta.238 (the beta tester's notes of 13.11, the owner: «посмотри глобально чтобы таких проблем небыло»): the
// shop's list head «Товар · Ціна · Вага» lay over its first row - the low HP edge's document has the body class «lh»
// and the theme's `.lh { position: absolute; }` took the shop's own «.lh» out of the flow. The theme's rules led by a
// window's body class must not reach an element of that class in another window: every such pair is found here, in
// the markup of the whole source tree. And the settings' list did not scroll by its bar - a press on RmlUi's
// scrollbar was taken for one on the window's ground (the window's drag).
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/ui_style.hpp"
#include "core/ui_theme_css.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::set<std::string> words(const std::string& s) {
    std::set<std::string> out;
    std::istringstream in(s);
    for (std::string w; in >> w;) out.insert(w);
    return out;
}

// The classes a selector's first compound names (".a.b:hover > .c" -> a, b)
std::set<std::string> leadClasses(const std::string& selector) {
    std::set<std::string> out;
    size_t i = selector.find_first_not_of(" \t\r\n");
    if (i == std::string::npos) return out;
    const size_t end = selector.find_first_of(" \t\r\n>", i);
    const std::string first = selector.substr(i, end == std::string::npos ? std::string::npos : end - i);
    for (size_t at = first.find('.'); at != std::string::npos; at = first.find('.', at + 1)) {
        size_t stop = at + 1;
        while (stop < first.size() && (std::isalnum(static_cast<unsigned char>(first[stop])) || first[stop] == '-' ||
                                       first[stop] == '_'))
            ++stop;
        if (stop > at + 1) out.insert(first.substr(at + 1, stop - at - 1));
    }
    return out;
}

// Every rule's first compound of the built-in theme, as its classes (".a.b:hover" -> {a, b}; none: not by class)
std::vector<std::set<std::string>> themeLeads() {
    std::string css = uitheme::builtInCss();
    for (size_t a = css.find("/*"); a != std::string::npos; a = css.find("/*", a)) {
        const size_t b = css.find("*/", a + 2);
        css.erase(a, b == std::string::npos ? std::string::npos : b + 2 - a);
    }
    std::vector<std::set<std::string>> out;
    size_t from = 0;
    int depth = 0;
    for (size_t i = 0; i < css.size(); ++i) {
        if (css[i] == '{') {
            const std::string selectors = css.substr(from, i - from);
            if (depth == 0 && selectors.find('@') == std::string::npos) {
                std::istringstream in(selectors);
                for (std::string one; std::getline(in, one, ',');) {
                    std::set<std::string> c = leadClasses(one);
                    if (!c.empty()) out.push_back(c);
                }
            }
            ++depth;
        } else if (css[i] == '}') {
            if (depth > 0) --depth;
            from = i + 1;
        }
    }
    return out;
}

struct Doc {
    std::string where;
    std::set<std::string> body;
    std::vector<std::set<std::string>> elements;  // each element's classes (its class="..." and data-class-<name>)
};

// Every RML document written in the sources (R"RML( ... )RML"): its body's classes and each element's
std::vector<Doc> sourceDocs() {
    std::vector<Doc> out;
    namespace fs = std::filesystem;
    for (const auto& e : fs::recursive_directory_iterator(fs::path(UAGFX_SOURCE_DIR) / "src")) {
        const std::string ext = e.path().extension().string();
        if (!e.is_regular_file() || (ext != ".cpp" && ext != ".hpp")) continue;
        std::ifstream f(e.path(), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        int n = 0;
        for (size_t a = text.find("R\"RML("); a != std::string::npos; a = text.find("R\"RML(", a + 1)) {
            const size_t b = text.find(")RML\"", a);
            if (b == std::string::npos) break;
            const std::string rml = text.substr(a + 6, b - a - 6);
            Doc d;
            d.where = e.path().filename().string() + "#" + std::to_string(n++);
            for (size_t t = rml.find('<'); t != std::string::npos; t = rml.find('<', t + 1)) {
                if (t + 1 >= rml.size() || rml[t + 1] == '/' || rml[t + 1] == '!') continue;
                const size_t close = rml.find('>', t);
                if (close == std::string::npos) break;
                const std::string tag = rml.substr(t, close - t);
                std::set<std::string> classes;
                for (size_t c = tag.find("class=\""); c != std::string::npos; c = tag.find("class=\"", c + 1)) {
                    if (c >= 1 && tag[c - 1] == '-') continue;  // data-attr-class: a binding's value
                    const std::string v = tag.substr(c + 7, tag.find('"', c + 7) - c - 7);
                    if (v.find('{') == std::string::npos) classes.merge(words(v));
                }
                for (size_t c = tag.find("data-class-"); c != std::string::npos; c = tag.find("data-class-", c + 1)) {
                    const size_t eq = tag.find('=', c);
                    if (eq != std::string::npos) classes.insert(tag.substr(c + 11, eq - c - 11));
                }
                if (tag.compare(0, 5, "<body") == 0)
                    d.body = classes;
                else if (!classes.empty())
                    d.elements.push_back(classes);
            }
            out.push_back(d);
        }
    }
    return out;
}

bool within(const std::set<std::string>& part, const std::set<std::string>& whole) {
    return std::includes(whole.begin(), whole.end(), part.begin(), part.end());
}

}  // namespace

TEST(ui_scopes_the_markup_is_found) {
    const std::vector<Doc> docs = sourceDocs();
    CHECK(docs.size() > 40);
    const auto shop = std::find_if(docs.begin(), docs.end(), [](const Doc& d) { return d.body.count("shl"); });
    CHECK(shop != docs.end() && std::any_of(shop->elements.begin(), shop->elements.end(), [](const auto& c) {
              return c.count("wr") && c.count("framed");
          }));
    const std::vector<std::set<std::string>> leads = themeLeads();
    CHECK(std::count(leads.begin(), leads.end(), std::set<std::string>{"shl"}) > 10);
}

TEST(ui_scopes_a_windows_class_reaches_no_other_window) {
    const std::vector<std::set<std::string>> leads = themeLeads();
    const std::vector<Doc> docs = sourceDocs();
    std::set<std::string> told;
    for (const std::set<std::string>& lead : leads) {
        std::string owner;  // a window whose body the rule's first compound picks
        for (const Doc& d : docs)
            if (within(lead, d.body)) owner = d.where;
        if (owner.empty()) continue;
        for (const Doc& d : docs) {
            if (within(lead, d.body)) continue;
            for (const std::set<std::string>& element : d.elements)
                if (within(lead, element)) {
                    std::string name;
                    for (const std::string& c : lead) name += "." + c;
                    if (told.insert(name + " " + d.where).second)
                        std::printf("  %s leads theme rules for %s and matches an element of %s\n", name.c_str(),
                                    owner.c_str(), d.where.c_str());
                }
        }
    }
    CHECK(told.empty());
}

TEST(ui_scrollbar_parts_are_controls) {
    for (const char* tag : {"scrollbarvertical", "scrollbarhorizontal", "slidertrack", "sliderbar", "sliderarrowdec",
                            "sliderarrowinc", "scrollbarcorner"})
        CHECK(uistyle::scrollbarPart(tag));
    for (const char* tag : {"div", "span", "body", "img", "", "slider"}) CHECK(!uistyle::scrollbarPart(tag));
}
