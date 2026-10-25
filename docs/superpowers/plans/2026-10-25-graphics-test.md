# Graphics test («Тест графіки») Implementation Plan

> **For agentic workers:** executed inline (superpowers:executing-plans), the user's standing rule: «все в инлайн
> режиме», beta phases without stops; releases only on «выпускай».

**Goal:** a player picks «Тест графіки» in the game's console (~); the driver runs a fixed 75-second test by itself
(each effects set, the same camera path every time), measures every set and sends the result as a report of kind
«тест» with a short table on top - no screenshots.

**Architecture:** core/graphics_test (host-tested) holds the schedule, the camera path, the measurements and the
table; the renderers cut their frame profile ([debug] profile's windows) at the test's phases on request
(Renderer::profileCut, the bridge's Msg::ProfileCut), the lines tagged «profile [test N/5 ...]»; win/context drives
the test each present (the set in place of the player's for the frame, the camera written into the game's CView,
keep_fps and the card's judge held), reads the tagged lines back from its logs at the end, writes
uaro-graphics\graphics-test.txt and starts `uaro-graphics.exe --test-report "<file>"`, which sends it without a
window (reason and kind "test") and writes `<file>.sent` (the code or the error) for the game's note.

**Spec:** the conversation of 2026-10-25 (the user: «есть также идея сделать некий режим теста в игре ... чтобы не
стоять кучу времени», then «делай без снимков. приступай»); my proposal there: the sets off / low / medium / high /
the player's own about 15 s each, the camera a full turn and out and back in on the same path, the profile on, the
player's settings back at the end, a report marked «тест» with a short table; the API and «Шейдер на кожен стан»
recorded, not switched (a restart each). The settings app's button is a later step.

## Global Constraints
- ddraw.dll imports only kernel32/user32/gdi32/msvcrt; lines <= 120; comments in English; UI texts en/uk/ru.
- The player's settings file is never written by the test; everything it changes is for the frame only.
- keep_fps never turns off what the player turned on by hand (unchanged: the test holds keep_fps, drops nothing).
- Site writes: the reason/kind "test" in GraphicsReport::REASONS/KINDS and the admin's labels (the user agreed to the
  report going to the site).

## Decisions (my calls)
- Phases: Off, Low, Medium, High (each the player's file with that set, every group on at 100 %) and Own (the
  player's settings as they are, without keep_fps's drops); each 3 s settling (not measured) + 12 s measured.
- Camera: yaw from the player's own, a full turn over the 12 s; distance near -> far -> near as a half sine; outdoors
  300..1400, a building map 230..300 (the game's own range there), a map of the game's own camera limits
  (CGameMode+0x284) its range; tilt untouched; the player's yaw and distance eased back at the end.
- Stops: the map changes or the world is left, the game window not in front, Pause, «Зупинити тест графіки» in the
  console; nothing is sent then.
- The table: per set the frames a second, the slowest tenth (as frames a second), the card's ms a frame and its three
  dearest parts (from the tagged profile lines; the card's timing's effects when the profile did not run).

## Tasks
1. core/graphics_test.{hpp,cpp} + tests/test_graphics_test.cpp: schedule (Run::frame), camera path (poseAt,
   zoomRange), results (PhaseResult), tagged-profile parsing (profileParts), the table (summary).
2. frame_profile::Window::lines(who, seconds, tag); Renderer::profileCut(label, byCuts) for Direct3D 11, Direct3D 12
   here and in uaro-render64 (Msg::ProfileCut).
3. core/game_camera.hpp: CView yaw/tilt offsets, CGameMode camera limits; win/game_camera: readCameraPose,
   writeCameraPose, readZoomRange.
4. Console row «Тест графіки» / «Зупинити тест графіки» (Kind::Test, takeTestAsk); Context: start, frame, stop, end;
   the frame counter's note shows the progress; strings.
5. app: --test-report "<file>" [--log-slot N] (Args::Mode::TestReport), reports::Kind::Test («Тест графіки» in
   «Звернення»), strings; tests for the arguments and the kind.
6. Site: "test" accepted; admin labels; bundle rebuilt.
7. Notes (uk/en/ru), version 3.0.5-beta.6, ship-beta.
