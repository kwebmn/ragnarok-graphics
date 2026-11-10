# uaUI possibilities (items 6-12 + A-D) - plan and ledger

Owner 2026-11-10: «надо сделать 6,7,8,9,10,11,12 а также А,Б,В,Г ... все в инлайн режиме без остановки».
Betas 3.0.18-beta.57 and on, one per stage. New looks off by default; every switch in the schema + the app
(interfaceSections). RML XML-checked before shipping ([[feedback-rml-checked-by-parser]]).

Facts (RmlUi 6.3 source):
- font size goes to the font engine truncated to an int (ElementStyle.cpp:1000);
- transitions fire only when the element's definition changes (class / pseudo-class), never for inline
  data-style changes (ElementStyle.cpp:501) - bars need our own easing;
- our bridge has geometry, textures, scissor, transform only (no shaders, layers, filters, clip masks).

## Stage 1 - letters always fit (8 / A)
- uaui::textWidth(utf8, px, bold): the drawing engine's own advances (the GDI faces, size truncated as RmlUi).
- panel::setMeasure(fn): core's wordsWidth asks the drawing engine when uaUI runs - plates sized by real letters.
- Sheets: a one-line word wider than its box shrinks to fit (down to 75 %), logged once per text.
- RULING: the windows' boxes stay laid out by their core code (the same boxes answer the mouse and the pad);
  flexbox only for parts with no clicks inside (tips, toasts, plates) - a flex window would draw buttons where the
  mouse does not find them. Cost if wrong: a window looks the same but text never spills either way.

## Stage 2 - motion (6, 7 / V)
- [interface] ui_motion = off | soft (off by default), app row under «Інтерфейс».
- soft: hover/press transitions on cells, buttons, rows (class changes), press scale 0.94, windows slide 6 px
  on open, bars eased (HP/SP/exp in char block, target, party, exp strip: shown value glides 200 ms), the low-HP
  plate pulses, the item toast slides in, the exp gain floats up.

## Stage 3 - components, binding, files (9, 10, 12)
- theme: input fields, select, range slider, scrollbars, tabs, buttons, checkbox in the game's look.
- uaui::Panel helper: a window = template + model in a few lines (the showcase rebuilt on it).
- uaro-graphics\ui\<part>.rml over any built-in part's markup (hot reload 1 s); ui\theme.rcss already.

## Stage 4 - the pad everywhere (11 / B)
- core/pad_nav: the next target in a direction (spatial, as RmlUi's nav: nearest by angle and distance) + tests.
- Content::target(box): clickable boxes; windows with no pad commands of their own fill them.
- runtime: the top window has targets and no handler -> D-pad moves the focus (gold ring), A clicks it (the
  part's own mouse: down + up at its middle), B closes.

## Stage 5 - the bridge (G: 13-16)
- gradients: CompileShader("linear-gradient"/"radial-gradient") -> per-vertex colours (subdivided for stops).
- soft shadows and glow: decorator ua-shadow(blur, colour[, spread]) - a feathered mesh.
- rounded clipping: overflow hidden + border-radius clipped on the CPU against the convex rounded box.
- the blurred glass behind windows: only if the renderer has a blurred copy of the frame to sample.

## Ledger
- Stage 1: Ruling: windows keep their core layouts (they answer the mouse and pad); fit by the drawing engine's
  measure + shrink-to-fit instead of flexbox in windows - a flex window would draw buttons where clicks miss them -
  cost if wrong: flex later for text-only parts only.
- Stage 1 shipped beta.57 (6796e64c). Stage 2 shipped beta.58 (cd67201d): party bars not glided (rows list) - later.
- Stage 3: Ruling: no separate uaui::Panel class - the data binding is already a few lines per window (Doc::make +
  Bind); the showcase's «Форма» shows two-way binding instead - cost if wrong: a helper later, nothing lost.
- Stage 3: keys go to a focused uaUI field (uaui::key) - the game may still see them through DirectInput (only the
  showcase has fields yet).
- Stage 3 shipped beta.59 (fe0287cf). Stage 4 shipped beta.60 (8cd0ff94): emotes, commands, chat room setup, chat
  room, popup, macros walked by the pad; whispers (pooled sheets) not yet.
- Stage 5: Ruling: the blurred glass (item 15) NOT built this round - it needs a copy of the frame blurred in both
  renderers (D3D12 and D3D11), FXC-safe shaders and a weak-card check; a separate piece of work - cost if wrong: one
  look missing, nothing broken. Built: linear gradients (radial/conic told once, not drawn), ua-shadow decorator
  (soft shadow / glow ring), rounded clip masks - all behind [interface] uaui_effects (off).
