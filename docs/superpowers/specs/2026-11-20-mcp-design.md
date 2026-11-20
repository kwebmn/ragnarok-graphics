# MCP in the client - design

**Owner (20.11):** «сделать в клиенте мсп сервер, через который можно было бы управлять клиентом и записывать видео»,
«чтобы ты мог ходить и делать правильные сцены», «могу пробросить тебе порт по ссш», «не надо заморачиватся, пускай
будет мсп у всех».

## Intent

Claude (the server's session, over the owner's SSH tunnel; or Claude Code in his WSL) drives the running client: puts
the character where a shooting sheet says, sets the camera, the time, the weather, Pause and the photo mode, looks at
the frame (a screenshot) and corrects it - the right scenes for the channel's videos; and checks the betas on the
owner's PC itself. Recording through OBS comes later.

## Decided

- In the public client, for everyone (no account binding, no key) - the owner's call.
- Safe for every player all the same: **off by default** ([mcp] enabled = 0, a switch in the settings), listening on
  **127.0.0.1 only**, requests with a browser's Origin or a Host other than 127.0.0.1/localhost refused (a web page
  must not drive a player's character - the MCP spec's own rule against DNS rebinding).
- Transport: MCP's streamable HTTP, POST /mcp with JSON-RPC 2.0, one JSON answer a request (no SSE); the owner's
  tunnel: `ssh -N -R 8770:127.0.0.1:8770 root@<loki>`; from the server plain curl works too.
- The protocol's logic in core (host-tested): HTTP request parsing and checks, JSON-RPC (initialize, ping,
  tools/list, tools/call, notifications), the tools' list with their input schemas, the arguments' checks.
- The game's work on the game's thread: the socket thread queues a call, the frame runs it, the answer goes back
  (10 s at most; a screenshot waits for its present).

## Stage 1 tools

state (map, cell, the 3D view and its look, Pause, photo mode, the clock), screenshot (JPEG, 1280 wide at most),
chat (a line through the game's own chat - @warp and the like), walk (to a cell, the pad's own request), view (the 3D
view: off / behind / first, its look yaw and pitch), pause (the old client on / off), photo (on / off), setting (a
line of uaro-graphics.ini - the clock, the weather, the season, any effect; applied live).

## Later

The free camera and its flights, the time-lapse, OBS's recording (its websocket), MCP resources with the shooting
sheets.

## Approaches weighed

1. A relay on our server, the client connecting out - no ports on the owner's PC, but a service to run. Dropped: the
   owner forwards a port by SSH himself.
2. Claude Code in his WSL only (stdio exe) - simplest, but that session lacks this one's history and cannot fix the
   client. Kept possible: the same HTTP server serves it (WSL reaches it with mirrored networking).
3. **Chosen:** an HTTP MCP server inside the driver on 127.0.0.1, reached through the owner's SSH tunnel.
