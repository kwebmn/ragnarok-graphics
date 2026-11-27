#pragma once
#include <map>
#include <string>

namespace uagfx::vendprices {

// 3.0.24-beta.23 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Відкрити лавку»: «цены
// запоминаются»): the prices a merchant put on things, by the thing's number, kept for the next shop - a text of
// «number=price» lines («#» a remark; a line not understood skipped); the file is core/layout's kVendPrices
class Prices {
public:
    void read(const std::string& text);
    std::string text() const;
    void remember(int id, long long price);  // a nought or less forgets nothing
    long long priceOf(int id) const;         // 0 none kept

private:
    std::map<int, long long> prices_;
};

}  // namespace uagfx::vendprices
