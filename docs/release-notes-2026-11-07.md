# WinBrew 11 / Vapor: combined desktop and CS2 update

Updated November 7, 2026. The combined application is installed and startup-verified on the owner's PS5. Current title: **PPSA77382**, version **01.016.000**. The release binaries are unchanged by this documentation update; `main` contains the latest source and guides.

## Working games

- **Schedule I works**, reported by the owner. Multiplayer/co-op has not been established by this report.
- **Counter-Strike 2 (app 730, called “CSGO” by the owner) works offline with bots.** LAN/local multiplayer may work but has not been tried. LAN-over-Internet projects exist, but tunneling is untested with this setup. In-game Steam authentication and official online matchmaking remain unsupported. Legacy CS:GO was not separately tested.

See the [compatibility matrix](https://github.com/KPKev/WinBrew-11/blob/main/docs/compatibility.md) for evidence and research links.

## Included improvements

- Persistent per-game artwork, descriptions/genres, and news cache. Cached content is reused across restarts; **Refresh game cache** requests the latest data for one game. Automatic discovery of new games remains enabled.
- Fixed expensive window-shadow rendering that caused mouse/controller dragging in Vapor. The owner confirmed smooth input after this fix.
- Reduced repeated text rendering, Recent Games sorting, shortcut lookups, and download-view copying. Further combined optimizations are installed; additional owner hands-on testing is pending. Host benchmark timings are not PS5 FPS claims.
- Automatic complete fixed `cs2-019.1` runtime selection during normal CS2 first installation/reinstallation, preserving existing bottle/settings/save data during normal owned-title refresh. Other games retain their runtime selection.
- Skyrim Special Edition, Terraria, Brotato, and Counter-Strike: Source default profiles. Saved settings retain precedence; profiles do not certify gameplay compatibility.

## Install

- **Existing Vapor setup:** download `Vapor-CS2-019.1.zip`. Close WinBrew and update the complete `/data/homebrew/PPSA77382` application, preserving private data and recorded Unix file modes. Keep your configured dedicated base kit.
- **New setup:** also download `Vapor-base-kit-0.13.zip`. Extract its `playden-kit` folder to `/data/wine11/kits/playden-kit`. Do not replace Playden's separate kit. A supported PS5 homebrew loader/payload environment is required.
- If extraction/FTP loses permissions, `python fix-vapor-install-modes.py --host YOUR_PS5_IP --base-kit` shows a read-only plan; add `--apply` to restore and verify the pinned modes. Omit `--base-kit` for an already configured kit. The helper changes no game/account/save contents.
- Follow [getting started](https://github.com/KPKev/WinBrew-11/blob/main/docs/getting-started.md). No games, accounts, or saves are bundled.

## Verification and source

All 1,856 installed app files were checked, including the 1,760-file runtime and notices; startup and input initialization passed. Host tests cover cache/reset/offline behavior, pixel-identical shadows/text, input, downloads, full runtime first-install/reinstall, missing/corrupt/symlink rejection, and save-preserving migration. The owner's complete game download was not repeated. [Current verification](https://github.com/KPKev/WinBrew-11/blob/main/docs/current-status.md) distinguishes these checks from gameplay acceptance.

`cs2-runtime-019.1.zip` is a source-build input already bundled in the application. `ntdll-cs2-019.1.prx` is the exact derived module; `cs2-runtime-corresponding-source.zip` supplies complete Wine/prospero/module-converter sources, patches, scripts, and generated configuration. Licenses accompany the repository and application. Check downloads with `SHA256SUMS.txt` and `release-verification.json`.

Pull **main** for current code. The [source-build guide](https://github.com/KPKev/WinBrew-11/blob/main/docs/source-build.md) documents native prerequisites and the full `tools/build-vapor-cs2.sh` target; a bare clone is not a one-command full native rebuild. The complete release ZIP is the immediate runnable installation path.
