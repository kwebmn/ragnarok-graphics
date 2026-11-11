#!/usr/bin/env python3
"""item_elements.py UATHENA OUT_CPP: the things' elements (3.0.18-beta.144, the owner: element things in their own
colour) from the server's own item_db*.txt - a weapon's attack element (`bonus bAtkEle,Ele_X`), an armour's or
a card's defence element (`bonus bDefEle,Ele_X`: the armour cards Pasana, Swordfish ... give it to the armour they go
in). Writes the C++ table core/item_elements.cpp reads: id -> element (RO's: 1 water, 2 earth, 3 fire, 4 wind,
5 poison, 6 holy, 7 shadow, 8 ghost, 9 undead).

Usage: python3 -I tools/item_elements.py /root/uAthena src/core/item_elements_data.cpp
"""
import re
import sys
from pathlib import Path

ELEMENTS = {"Ele_Water": 1, "Ele_Earth": 2, "Ele_Fire": 3, "Ele_Wind": 4, "Ele_Poison": 5, "Ele_Holy": 6,
            "Ele_Dark": 7, "Ele_Ghost": 8, "Ele_Undead": 9}


def table(texts):
    out = {}
    for text in texts:
        for line in text.splitlines():
            if not line or line.startswith("//") or not line[0].isdigit():
                continue
            item = int(line.split(",", 1)[0])
            m = re.search(r"bonus\s+b(Atk|Def)Ele\s*,\s*(Ele_\w+)", line)
            if m and m.group(2) in ELEMENTS:
                out[item] = (ELEMENTS[m.group(2)], m.group(1) == "Atk")
    return out


def main():
    root, dest = Path(sys.argv[1]), Path(sys.argv[2])
    texts = [(root / "db" / n).read_text(encoding="latin-1") for n in ("item_db.txt", "item_db2.txt")
             if (root / "db" / n).exists()]
    t = table(texts)
    rows = "\n".join(f"    {{{i}, {e}, {'true' if atk else 'false'}}}," for i, (e, atk) in sorted(t.items()))
    dest.write_text(f"""// Made by tools/item_elements.py from the server's db/item_db*.txt - do not edit by hand.
#include "core/item_elements.hpp"

namespace uagfx::itemelements {{

const Entry kTable[] = {{
{rows}
}};
const size_t kTableSize = sizeof kTable / sizeof kTable[0];

}}  // namespace uagfx::itemelements
""")
    print(f"{len(t)} things with an element")


if __name__ == "__main__":
    main()
