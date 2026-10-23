# The Client's Graphics Updates - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (inline, the user's way); self-review.

**Goal:** the admin's GRFs in `<client>\updates\` over the game's archives, for the game and the driver's own readers;
3.0.2-beta.5 on «Бета».

**Spec:** `docs/superpowers/specs/2026-10-23-graphics-updates-design.md`.

## Tasks

1. core/updates: `lookupKey` (ASCII lower-case, CP949 trail bytes kept, '/' as '\'), `updateOrder` (by name, the
   later winning), `Index` (key -> archive, stored name; the later archive wins), `archiveOrder` (updates newest
   first, then DATA.INI's). core/updates_exe: the three functions' addresses and prologues. Tests
   `tests/test_updates.cpp`.
2. win/updates_hook: the three entry hooks (9-byte prologues, trampolines), lazy open, the lock, VirtualAlloc(size +
   8) buffers; log lines for the driver's start; installed at DllMain.
3. The driver's map loader, the pack builder and the catalog: updates\ first.
4. `[client] updates`, schema row, texts, template; «Сервіс» section with the folder's count and size.
5. Notes, gate, ship-beta.
