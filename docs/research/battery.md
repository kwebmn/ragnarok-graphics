# Playing on a laptop's battery

A laptop spends on the game in three places: the GPU drawing frames, the processor kept awake (frequent wake-ups, busy
waits), the screen. The client never waits by itself (see `frame-and-hooks.md`): the driver's present is what paces it.

## Settings that exist now

- `[frames] fps_limit = 30` and `vsync = 1`: about half the GPU's and the processor's work of 60 frames.
  `save_power = 1` (on by default) keeps 15 frames in the background and 30 at the login.
- The effects at Low or off. On integrated graphics the full-screen passes cost most: the light from the sky, the
  shadows, the glow, the bloom, the depth of field. `keep_fps` keeps frames, not watts.
- `[quality] world_scale = 75` (Direct3D 11) or `[display] high_resolution = 0`: fewer pixels - after the frame rate the
  largest saving on a 2880x1800 screen at 200 %.
- `[ai] enabled = 0`, or the sprites and icons only: x4 pictures take memory bandwidth, which integrated graphics share
  with the processor.
- Off: `fps_counter` (its layered window is drawn again), `[debug] profile` (1000 wake-ups a second), `log = verbose`,
  DLSS and frame generation, `render_thread` (one thread fewer to wake).
- On a laptop with two cards, `[renderer] gpu` = the integrated one (and Windows Settings, Graphics, uaRO.exe, Power
  saving): enough for a 2007 game at Low, and the discrete card is the hungriest part of the laptop.

## Windows

The panel at 60 Hz (120-144 Hz costs more), lower brightness, the Balanced or Power saver plan, battery saver.

## A battery mode in the driver (not built)

The driver knows mains or battery since 2.5.15 (`core/computer.hpp`); a check every few seconds would follow a cable
pulled out. On battery, automatically or by a button in the settings app (undecided):

- a frame cap of 30 (configurable), the effects at Low, `world_scale` 75 - all applied in a running game;
- `FramePacer` without its spin: now it spins the last half millisecond of each frame (`Sleep(0)`); on battery it would
  sleep through on its waitable timer - half a millisecond of jitter is invisible at 30 frames;
- the client's `timeBeginPeriod(1)` (set in `WinMain`) undone: the whole system wakes 1000 times a second for it, and
  the pacer uses a high-resolution waitable timer anyway; the client's tick counter `FUN_00421e90` is fine at 4 ms;
- in the background and minimised as now (15 frames), and nothing drawn while the window is hidden.

Expected, without measurements (no laptop at hand): about 2-3 hours of play instead of one.

## Built (2.5.20)

`[frames] battery_saver` (off by default) and `battery_fps` (30, 15-60) on the Frames page. The driver asks Windows
every 3 seconds whether the laptop runs on its battery (`GetSystemPowerStatus`); while it does: `battery_fps` frames at
most, the effects at most the weak set's (each switch on only if the file has it on and the weak set has it), the world
at 75 % at most (Direct3D 11), the frame pacer sleeping on its timer without the last half millisecond's spin, and 5
turns of the game's loop a second when it is minimised (15 before). On the mains everything is as the file says.

Not done: undoing the client's `timeBeginPeriod(1)` - since Windows 10 2004 a process's timer resolution touches no
other process, and the pacer falls back to an ordinary timer before 1803, whose half-millisecond accuracy depends on it.
