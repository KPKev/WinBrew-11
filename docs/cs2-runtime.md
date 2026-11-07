# Automatic CS2 runtime

Current combined app: **PPSA77382 / 01.016.000**, installed and startup-verified November 7, 2026. Start with [getting started](getting-started.md), [current verification](current-status.md), and the [compatibility matrix](compatibility.md). The release also contains the persistent game cache and desktop/Vapor efficiency fixes.

Use the **Vapor-CS2-019.1.zip** application from the `cs2-runtime-v1` GitHub release. Keep the entire app folder together: the fixed runtime is inside it. Copying only `eboot.bin` does not install this feature.

On a supported homebrew-enabled PS5, install the app folder as `/data/homebrew/PPSA77382`. Existing Vapor users keep their existing app account/state and base kit. New users also need the release's `Vapor-base-kit-0.13.zip`, extracted so its `playden-kit/manifest.json` is at `/data/wine11/kits/playden-kit/manifest.json`. Preserve archive file modes (directories 0755; individual files 0644/0755 as recorded in the kit manifest). Use a homebrew installer/extractor that preserves Unix modes; a plain FTP copy may require correcting modes. Do not replace another application's `/data/playden` kit. The normal PS5 homebrew loader, its supported system version and payload prerequisites still apply.

Sign into Steam in Vapor to access your own library, then install Counter-Strike 2 normally. Vapor recognizes **Steam app 730** and stages the complete matched `cs2-019.1` runtime into that game's `win/wine` directory before publishing the title. Removing and reinstalling CS2 follows the same preparation path. Existing owned CS2 installations can refresh through normal preparation; their bottle, settings and saves remain in place. A missing, linked or modified bundled runtime fails preparation instead of falling back to the older runtime.

If your file manager/FTP copy loses Unix file modes, download `fix-vapor-install-modes.py` from the release and run it on your PC after copying/extracting the files. With Web File Manager running on the PS5, `python fix-vapor-install-modes.py --host YOUR_PS5_IP --base-kit` shows a read-only plan; add `--apply` to restore and verify the recorded modes. Omit `--base-kit` if your existing dedicated base kit is already configured. This helper accepts only this release's pinned manifests and changes no file contents, downloaded games, accounts or saves.

The default shared kit remains 0.13. Other Steam games and local imports retain their existing runtime selection. The Skyrim Special Edition, Terraria, Brotato and Counter-Strike: Source default profiles are also included; saved user settings retain precedence. No game downloads, game files, Steam accounts or saves are included in this release.

## What the fix contains

The matched runtime is Playden kit 0.19's Wine directory, with one derived `ntdll.prx`. The newer matched runtime resolves the startup file-descriptor exhaustion. The derived module rejects guest fixed virtual-address hints overlapping the PS5's 32–64 GiB system band, allowing Wine/V8's existing retry path, and checks that a fixed reservation actually returned the requested address. It does not rewrite CS2 executables.

The owner confirmed CS2 offline gameplay with bots on this runtime. **Online Steam authentication/matchmaking remains unsupported by the in-game runtime.** Vapor's Steam login is used for its library/download services and does not supply an authenticated Steam client to the game.

LAN/local multiplayer may work but has not been tried. LAN-over-Internet projects are research leads, not verified CS2/PS5 support. The tested game is CS2/app 730, not a separately tested legacy CS:GO executable. Schedule I is also owner-reported working; see the compatibility guide for the exact scope of these reports.

`assets/cs2-runtime/manifest.json` pins every runtime file. Its digest and file list are compiled into `src/steam_integration/cs2_runtime_files.hpp`. The actual installer uses scoped mappings, verifies source layout/mode/size and copied bytes, and records the selected runtime's inventory identity. It uses the same controlled publication and recovery path as ordinary installation.

## Downloads and source

- `Vapor-CS2-019.1.zip`: complete application, including the fixed runtime and license notices.
- `Vapor-base-kit-0.13.zip`: dedicated base kit needed on a new Vapor setup; existing installations need no base-kit change.
- `cs2-runtime-019.1.zip`: independently verifiable runtime payload for source builds. Already included in the application download.
- `ntdll-cs2-019.1.prx`: exact derived module for inspection/rebuilding; no manual replacement is required when using the application download.
- `cs2-runtime-corresponding-source.zip`: complete Wine/prospero/module-converter source archives, patch, original build scripts, generated configuration and audit details for the changed module.
- `SHA256SUMS.txt`: checksums for release assets.

The app itself is built from this repository and the pinned inputs in `dependencies.json` and `tools/steam-network-tested-inputs.json`. Existing source-build setup is still needed for the PS5 SDK, fonts, native app base, network archives and dedicated host libraries. After that setup, run `bash tools/build-vapor-cs2.sh NEW_BUILD_NAME`. This invokes the full Steam/Vapor build, not the older desktop-only `build-ps5.sh` target. It fetches and verifies the fixed runtime automatically; `python tools/prepare-cs2-runtime.py --verify-only` audits an existing runtime cache. It performs no console deployment. To create the distributable ZIP with the recorded Unix modes, run `python tools/package-vapor-cs2-release.py build/NEW_BUILD_NAME-isolated/PPSA77382 NEW_OUTPUT.zip`.

For the runtime change, start with the corresponding-source archive and `runtime-patches/cs2/README.md`. The complete source is offered alongside the binaries; original upstream notices and source revision references are in `runtime-patches/cs2/upstream-notices/`. The source tree and scripts allow rebuilding/replacing the modified LGPL module; no proprietary game content is needed.

## Verification

The real filesystem installer suite covers first installation, removal/reinstallation, migration with a saved file preserved, other Steam apps, local imports and missing/corrupt/symlink rejection. Runtime assets and compiled pins are checked again by package assembly. The PS5-native host and complete app are separately compiled and linked. Offline gameplay acceptance applies to the tested runtime, rather than claiming a fresh 75 GB game download was repeated on the owner's console.
