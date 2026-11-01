# The game's remaining windows in the new look - Implementation Plan

> For agentic workers: executed inline (the owner: «сделай все без остановки, в инлайн режиме»), a beta per task.

**Goal:** every game window the player still sees in the old look redrawn in style A (the concept «Концепция 2.3»),
as «Речі», «Склад», the menu were.

**Architecture:** each window as the earlier ones: core/<window> (layout, paint, words, hit test - tested on the host),
win/<window> (the game's window found at its manager's place by its class's vtable, its shown flag held at 0, its
buttons pressed through its own SendMsg, the mouse/keys/pad routed through win/window_stack), an [interface] switch
(config, schema row on «Інтерфейс», console list, strings uk/ru/en), on with the new minimap like [interface] menu.

**Facts:** the game's windows by id, manager offset and constructor - table from FUN_004d42d0 (scratch
makewindow.c); WNDINFO names in uaRO.exe name them. Descriptions: items data\<lang>\IdNum2itemDescTable.txt /
Num2itemDescTable.txt, the language folder by GetUserDefaultLangID (7 german, 0x10 italian, 0x16 portuguese, 0x1f
turkish, else english - 0x4EF2xx); skills data\english\skilldesctable2.txt (the root table is French).

## Tasks (a beta each)

1. Item card (window 0xC, mgr+0xC8, class 0x698D88: ITEM_INFO at +0x60) and skill card (0x2E, mgr+0xD4, class
   0x698EB8: AEGIS name std::string at +0x74) - [interface] cards.
2. Small windows: the trade request (EXCHANGEACCEPTWND), the NPC's number and text input.
3. Login screens: login, server select, character select, character make.
4. Merchant: cart, vending make, own shop, buying from a vending shop.
5. Messenger: friends, whisper windows, chat rooms.
6. Guild window, party settings.
7. Identify, card compound, item collection, refine display.
8. Options (Alt+O), emotions list.
9. Pet, homunculus, mercenary.

Each task: research (decomp: the window's data and buttons) -> core with tests (RED first) -> win part -> strings
(uk column quoted to the owner) -> build host + win -> gate -> notes ru/uk/en -> ship-beta.sh -> memory.
