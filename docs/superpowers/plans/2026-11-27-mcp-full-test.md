# MCP для полного теста на ноутбуке — план

> Inline execution (the owner's rule), betas by gship. Steps: test first, watch it fail, code, watch it pass, commit.

**Goal:** через MCP (без SSH) я управляю ноутбуком владельца: игра (её MCP, порт 8770) и сторож в программе
настроек (свой MCP, порт 8771) — замеры производительности, ошибки, проверка окон, состояние, настройки, перезапуск
игры и Direct3D 11/12.

**Spec (the owner, 27.11 23:00–23:10):** «запустить тестирование на ноутбуке через мсп, чтобы ты мог управлять им
удаленно, протестировать все возможности, видел проблемы с производительностью и все возможные моменты»; ответы:
ноутбук его (туннель его ключом к loki), перезапускать игру можно, ноутбук стоит долго, видео не писать; «давай без
встроенного ссш, давай все через mcp».

## Global Constraints

- Только 127.0.0.1 (туннель `ssh -N -R 8770:127.0.0.1:8770 -R 8771:127.0.0.1:8771 root@loki.kweb.biz`); проверки
  Host/Origin как у игрового MCP (core/mcp allowed).
- Драйвер не импортирует ws2_32 (GetProcAddress); программа настроек — тоже без новых импортов (LoadLibrary /
  GetProcAddress), список tools/check_exe.py не трогаем.
- Вызовы игровых инструментов — на потоке игры, ответ ≤ 10 с: замер берёт накопленное окно, а не ждёт.
- Строки ≤ 120; тексты (если есть) на трёх языках; беты `gship.sh 3.0.24-beta.N`; карточки «Перевіряємо зараз».

## Stage A — the game's MCP (beta .34)

1. **core/frame_stats** — the game's presents of the last 60 s: `stats(seconds)` → frames, fps, ms p50/p95/p99/max,
   over 25/50/100 ms, the longest ones' moments. Fed at each present (Context, beside the frame counter). Test
   `frame_stats_percentiles_and_stalls`.
2. **`perf {seconds 1-60}`** — frame_stats + the renderer's last GpuTiming (gpu ms, passes, present, cpu, upscaler,
   reflex, frame generation, loading) + memory (the game's process, the renderer's) + API and adapter + power
   (mains/battery %) + window + map. Its JSON made in core (`mcpperf::report`). Test `mcp_perf_report`.
3. **`log {file graphics|render64|pad, since "hh:mm:ss"}`** — `mcp::linesSince`. Test `mcp_log_since`.
4. **`ui {check}`** — the visible uaUI documents; `check`: words under 3:1 contrast, words cut by their box
   (uikit/ui_check, shared with the host tests' walker). Test `uikit_ui_check_finds_faint_and_cut_words`.
5. **`game {what character|items|chat}`** — HP/SP, weight, zeny, levels; the bag/cart/storage (itemstate); the chat's
   last lines with their kinds. Test `mcp_game_args`.
6. **`settings {prefix}`** — every known key: value now, the values allowed. Test `mcp_settings_list`.

## Stage B — the watchdog in the settings app (beta .35)

7. **core/mcp catalogs** — `handle()` with a server's own tools; the watchdog's table and checks (core/mcp_watch).
   Test `mcp_watch_catalog`.
8. **app/mcp_watch** — its server thread (127.0.0.1:port+1) while the app runs; the app stays (tray) when [mcp]
   enabled. Tools: `game {status|start|stop|kill|restart}`, `system {seconds}` (CPU of the system, the game, the
   renderer; memory; power; nvidia-smi when there), `files {list|tail|get}` (uaro-graphics\ only), `ini {get|set}`.
9. **loki**: mcp.py for both ports; the test plan; the run; the findings' document.
