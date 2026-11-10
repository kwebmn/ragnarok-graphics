#include "core/text_field.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.72: the new interface's fields - any language, a letter removed whole, a selection replaced
TEST(text_field_types_any_language_and_removes_whole_letters) {
    textfield::Field f;
    CHECK(textfield::type(f, "\xD0\x9C\xD0\xB5\xD1\x87", 64));  // «Меч»
    CHECK(textfield::type(f, "x", 64));
    CHECK_EQ(f.text, std::string("\xD0\x9C\xD0\xB5\xD1\x87x"));
    CHECK(textfield::backspace(f) && textfield::backspace(f));
    CHECK_EQ(f.text, std::string("\xD0\x9C\xD0\xB5"));  // «Ме»: «ч» went whole, not one byte of it
    f.all = true;
    CHECK(textfield::type(f, "a", 64));
    CHECK_EQ(f.text, std::string("a"));
    CHECK(!f.all);
    f.all = true;
    CHECK(textfield::backspace(f) && f.text.empty());
    CHECK(!textfield::backspace(f));
    // at most `most` bytes, never half a letter
    CHECK(textfield::type(f, "ab", 3));
    CHECK(!textfield::type(f, "\xD1\x87", 3));  // 2 bytes more do not fit
    CHECK_EQ(f.text, std::string("ab"));
    CHECK_EQ(textfield::oneLine("a\r\nb\tc\x01"), std::string("a  b c"));
}
