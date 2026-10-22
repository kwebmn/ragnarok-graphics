# Update channels on GitHub - design

Status: proposed 2026-10-23, waiting for the user's approval. It changes two decisions of the beta design
(`2026-10-22-beta-release-design.md`): 4 (no channels) and 14 (updates from uaro.kiev.ua).

## Intent

The user (2026-10-23): «а мы можем с гитхаба лить релизы. и наверное надо сделать две ветки драйвера - релиз и бета, в
бете будем делать тесты и обновления а релиз для остальных игроков», then «у нас могут быть обновления самого клиента
игры, графики, монстров, итемов. вот это надо сделать через сайт».

Why now: the first run of 3.0.0-beta.1 said «Не вдалося перевірити» on the Service page - the app asks
uaro.kiev.ua/files/graphics/update.ini, which is only written at a release (404 today). Testing and fixing a beta must
not touch the players, and a driver release must not need a write to the site.

## Two kinds of updates, two sources

| What | Where from | Channels |
|---|---|---|
| uaRO Graphics (ddraw.dll, uaro-graphics.exe, the renderer, DLSS, the AI models) | GitHub, kwebmn/ragnarok-graphics | «Реліз» = branch `main`, «Бета» = branch `beta` |
| The game's own files (graphics, monsters, items, maps) | uaro.kiev.ua, admin «Оновлення клієнта» (phase 6, unchanged) | none: every player at once (they must match the server) |

## Goals

1. Each branch carries, in `release/`, an `update.ini` (format 1 as today: [release] version, withdrawn; [package],
   [dlss], [models] with file, size, sha256) and the archives. The app reads the update.ini of its channel straight from
   raw.githubusercontent.com (no GitHub API, so no limit of 60 requests an hour) and downloads the archives from there
   (checked 2026-10-23: Range answers 206, cache max-age 300 s).
2. Testers get beta builds (`X.Y.Z-beta.N`) by the app's own update; players get only release builds (`X.Y.Z`).
3. A release is a beta that was tested: `main` fast-forwards to a release commit of `beta`; both channels then offer the
   same version.
4. Players on 2.8 and older keep updating the old way: they read `src/core/version.cpp` and
   `release/uaro-graphics.zip` on `main` - the 3.0.0 release reaches them there through the transitional install.bat.
5. The «Сервіс» page shows the channel: «Канал оновлень: Реліз / Бета (тестування)».

## How a computer gets its channel

- A fresh install of a beta build (the setup or a full client built from `beta`) is on «Бета»; of a release build, on
  «Реліз».
- Installing a beta build over an install puts it on «Бета» (that is how a tester joins); installing a release build
  never changes the channel - a tester updated to a release stays on «Бета».
- The player switches on the «Сервіс» page; the check runs again at once.
- Leaving «Бета» for «Реліз» never offers an older version: the install waits for a release newer than its build.

## Non-goals

- Programs through the game-files patcher (uaRO.exe, .dll): still refused. The user: «может появится какаято
  необходимость, но не факт» - added when it is needed.
- A beta of the game's files.
- A fallback to uaro.kiev.ua when GitHub does not answer: the check says it could not, as it does today.
- GitHub Releases (they need an API token; the deploy key can only push).

## Anti-goals (a failure even when it works)

- A beta build offered to a player on «Реліз», or to a 2.8 player (a `-beta` version on `main`).
- An update that fails because the CDN gave a new update.ini with an old archive (or the reverse).
- A tester dropped from «Бета» by an update.
- A driver release that needs a write to the site.

## Decided by the user

- Two branches, release and beta; beta for tests and updates, release for the other players.
- The driver's releases from GitHub; the game's files through the site.
- No programs through the patcher for now.

## My calls (to check)

1. The archives are named in update.ini by the commit that holds them, so their URL never changes
   (`https://raw.githubusercontent.com/kwebmn/ragnarok-graphics/<commit>/release/<file>`): a cached update.ini always
   names archives that match it. A release is two commits: the archives, then update.ini naming the first.
   `release/uaro-graphics.zip` keeps its fixed name for the 2.8 updater.
2. No cache-busting query on GitHub requests: a new version shows within 5 minutes; GitHub's origin is spared.
3. I push beta builds to `beta` myself, without asking each time; `main` only on «выпускай».
4. Development continues on the local `beta` branch; local `main` follows GitHub's `main` (now 2.8.0).
5. The channel switch is shown in the Detailed and Developer modes, not in Simple.
6. The report («Звернення» and the automatic one) names the channel.
7. Version numbers: the testers' builds are `3.0.0-beta.N`; the players' first one is `3.0.0`. The waiting fixes (HDR
   off by default, the crash-report question) go out as `3.0.0-beta.2`, the first build on the beta channel.
8. uaro.kiev.ua/files/graphics is no longer used; `tools/publish_site.py` and its step in the release script go.
9. The existing 3.0.0-beta.1 installs read the site: they are updated by hand once (or by the WSL client build).

## Left to the builder

The update.ini key for the commit (a [release] `ref` or per archive), how the installer and the package mark a beta
build's channel, the release scripts (beta and release), the texts, the tests.
