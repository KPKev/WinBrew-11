# Vapor credential model: run as the app user, like Playden

## Why
Playden's backend runs with the **app user's effective UID (uid 1)** and raises root only around specific steps (`playden::ps5::WithRoot`). Everything it creates for a game (title folder, `game-launch.json`, `.playden-control`, publication lock and state, bottle, game folder) belongs to uid 1. The title launcher reads `/app0/game-launch.json` **before** its own grant, as uid 1, and after the grant its publication reader requires every file to belong to its effective UID (also uid 1).

Wine 11 kept its whole process at root after `elevation::request`, so Vapor published every game as root. Result: every Vapor-installed game failed at startup with `invalid-descriptor`, which showed as a SIGSYS screen. Measured on the console 2026-10-29 (`evidence/skyrim-tree-owner-census.json`): Quake (Playden-published) 1779/1779 entries uid 1; Skyrim and Blackjack (Vapor-published) 100 % uid 0. After handing Skyrim's files to uid 1, Skyrim started and reached its launcher.

On this console, root **cannot enter a 0700 directory owned by another user**, and elfldr reports `uid=1 euid=0`.

## What changed
| File | Change |
| --- | --- |
| `src/ownership_adopt.{hpp,cpp}` | Post-order, symlink-safe, same-device hand-over of uid-0 entries to the app user; restores 0711→0700 on private dirs; strict title allow-list parser. |
| `src/steam_integration/app_user_identity.{hpp,cpp}` | `AdoptAndLower`: after the grant, a one-time adoption of Vapor's files, then `seteuid(app user)` and a verify. If adoption is incomplete or lowering fails, Vapor **stays root**, as before, and retries on the next start. A marker in the private dir records completion. |
| `src/privilege.{hpp,cpp}` | `ww::privilege::run(step)`: runs one filesystem call through an installed raiser; with none installed (desktop, preview, tests) it runs directly. |
| `src/steam_integration/vapor_privilege.{hpp,cpp}` | Vapor's raiser: `TryHoldEffectiveUid` + `WithRoot`, and **never blocks the UI** (EBUSY if the backend holds the credential lock). |
| `src/explorer.cpp`, `explorer_fileops.cpp`, `notepad_document.cpp`, `image_codec.cpp` | Only the privileged calls (open, opendir, stat probe, fchmod, rename, unlink, fopen) go through `ww::privilege::run`. |
| `tools/prepare-steam-native-experiment.py` | Game-host startup: record `geteuid()` before the grant; after the `/data` anchor and before any Steam/Playden thread, call `AdoptAndLower`, log the outcome, install `VaporRaise` when lowered. |
| `tools/probe-steam-native-pie.sh` | Compiles `app_user_identity`, `vapor_privilege` and `ownership_adopt` into the game-host build. |
| `tools/stage_build.py`, `tools/build-preview.cmd`, `tools/test.sh` | Build and test lists include `privilege.cpp` and the new tests. |

### What the one-time adoption touches
- The app's private directory (Vapor state `wine11-steam/`, settings, log).
- `/data/wine11/wine11-games` (mode back to 0700) and everything in it.
- `/data/playden/games/wine11-steam-*`.
- `/data/homebrew/<ID>` for **exactly** the IDs in `/data/wine11/title-rule/allow-list`.

Left as root, matching Playden: `/data/wine11`, `/data/wine11/kits`, `/data/wine11/title-rule`, the Wine 11 app folder, shared parents and Playden's own games.

## Tests
- `tests/ownership_adopt_test.cpp`, `tests/app_user_identity_test.cpp` (real root; ASan/UBSan clean). Coverage: interrupted runs, foreign owners, symlinks, entry limits, stay-root-on-failure, marker skip, and that untouched trees stay root.
- `tests/privilege_test.cpp`.
- Every existing desktop C++ test in `tools/test.sh` (lines 130–210) passes with the change: 31/31, run in a root Linux container.

## Not verified yet (needs the console)
- PS5 build and link of the new objects.
- Real `seteuid` behaviour after the lapy grant on 13.60. Playden relies on it, but Wine 11's helper path has to be measured.
- Explorer, Notepad and Paint on protected folders as uid 1 with raised opens.
- Fresh downloads through Vapor producing uid-1 titles that launch.

## Out of scope
Playden's SteamStub unpacking is unchanged and was not worked on.
