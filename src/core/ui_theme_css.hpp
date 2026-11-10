#pragma once

namespace uagfx::uitheme {

// 3.0.18-beta.94: the built-in theme of uaUI (win/uaui) moved here from it - the host tests lay the live windows
// out with it (tests/rml_host)

// The built-in theme: the new interface's look (the concept's colours, 8dp corners, 0.12-0.16 s eased out).
// uaro-graphics\ui\theme.rcss in its place when there - read again within a second of a change.
const char* builtInCss();

}  // namespace uagfx::uitheme
