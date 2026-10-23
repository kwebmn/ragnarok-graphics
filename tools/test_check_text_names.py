#!/usr/bin/env python3
"""Tests of tools/check_text_names.py (2.15): every «name» in the settings app's texts is a name of its interface."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_text_names  # noqa: E402

SOURCE = r'''
const Entry kTexts[] = {
    {"&Apply", "&Застосувати", "&Применить"},
    {"Fixed in {1}", "Виправлено в {1}", "Исправлено в {1}"},
    {"Press «Apply»", "Натисніть «Застосувати»", "Нажмите «Применить»"},
    {"See «Fixed in 2.13.1» and «Add a file…»", "Див. «Виправлено в 2.13.1»",
     "См. «Исправлено в 2.13.1»"},
    {"Add a file…", "Додати файл…", "Добавить файл…"},
    {"Open «Unknown»", "Відкрийте «Невідоме»", "Откройте \"x\" «Невідоме»"},
};
'''


# 3.0.2-beta.6: each entry carries its Text id, the table constexpr (strings.cpp checks the order when built)
SOURCE_IDS = r'''
constexpr Entry kTexts[] = {
    {Text::Apply, "&Apply", "&Застосувати", "&Применить"},
    {Text::PressApply,
     "Press «Apply»", "Натисніть «Застосувати»",
     "Нажмите «Применить»"},  // 2.15
};
'''


class CheckTextNames(unittest.TestCase):
    def test_reads_the_entries_with_their_ids(self):
        entries = check_text_names.entries(SOURCE_IDS)
        self.assertEqual(entries, [("&Apply", "&Застосувати", "&Применить"),
                                   ("Press «Apply»", "Натисніть «Застосувати»", "Нажмите «Применить»")])

    def test_reads_the_entries(self):
        entries = check_text_names.entries(SOURCE)
        self.assertEqual(len(entries), 6)
        self.assertEqual(entries[2][1], "Натисніть «Застосувати»")
        self.assertEqual(entries[2][2], "Нажмите «Применить»")
        self.assertEqual(entries[5][2], 'Откройте "x" «Невідоме»')

    def test_a_known_name_passes(self):
        problems = check_text_names.check(check_text_names.entries(SOURCE)[:5], allow=set())
        self.assertEqual(problems, [])  # a mnemonic, a {1} and a trailing … are no part of a name

    def test_a_name_of_another_language_fails(self):
        texts = [("Apply", "Застосувати", "Применить"), ("Press «Застосувати»", "x", "y")]
        self.assertEqual([p[1] for p in check_text_names.check(texts, allow=set())], ["Застосувати"])

    def test_a_declined_name_and_a_value_pass(self):
        texts = [("Quality", "Якість", "Качество"), ("Seams", "Стики трави й доріг без сходинок", "Стыки"),
                 ("x", "Найкраще з «Якості»; смужки «Стиків трави й доріг»; профіль «{1}»", "y")]
        self.assertEqual(check_text_names.check(texts, allow=set()), [])

    def test_an_unknown_name_fails(self):
        problems = check_text_names.check(check_text_names.entries(SOURCE), allow=set())
        self.assertEqual(sorted({p[1] for p in problems}), ["Unknown", "Невідоме"])
        self.assertEqual(check_text_names.check(check_text_names.entries(SOURCE), allow={"Unknown", "Невідоме"}), [])


if __name__ == "__main__":
    unittest.main()
