# WinBrew 11 / Vapor

A Windows 11-inspired desktop and Steam game library for PS5 homebrew, created by **KPKev** with AI development assistance. Supported Windows games use Playden's Wine-based PS5 runtime. WinBrew 11 is an independent homebrew application; it does not install Microsoft Windows.

**Updated November 7, 2026:** the combined update is installed and startup-verified on the owner's PS5. The current application is **PPSA77382**, version **01.016.000**. This repository contains its current source, tests, runtime-selection logic, and build tools.

## Working games

| Game | Confirmed result | Multiplayer scope |
| --- | --- | --- |
| **Schedule I** | Working, owner-reported. | Multiplayer/co-op has not been established by this report. |
| **Counter-Strike 2 (Steam app 730, called “CSGO” in owner reports)** | Offline gameplay with bots works. The complete fixed runtime is selected automatically during normal CS2 installation/reinstallation. | LAN/local multiplayer **may work but is untested**. Internet tunneling is also untested. In-game Steam authentication and official online matchmaking remain unsupported. |

The tested Counter-Strike installation is CS2/app 730, not a separately tested legacy CS:GO executable. See the [compatibility matrix and LAN research notes](docs/compatibility.md).

## Get the latest runnable version

1. Open the [latest release](https://github.com/KPKev/WinBrew-11/releases/latest) and download the complete application ZIP, currently **Vapor-CS2-019.1.zip**.
2. Follow [getting started](docs/getting-started.md). New installations also need the dedicated **Vapor-base-kit-0.13.zip**; existing configured installations retain their base kit and private app data.
3. Keep the complete **PPSA77382** folder and its recorded file permissions. Copying only `eboot.bin` omits the bundled CS2 runtime. The release supplies a permission-repair helper, checksums, licenses, and corresponding source.

Clone or pull **main** for current code and documentation. SDKs, downloaded runtimes, and generated outputs are separate from Git. The [source-build guide](docs/source-build.md) explains the prerequisites and full target, `tools/build-vapor-cs2.sh`. The historical desktop-only `build-ps5.sh` is not the complete current Steam/Vapor application.

## Included in the combined update

- Steam sign-in, automatic game discovery, downloads, and per-game preparation through the existing Playden services.
- Persistent per-game hero images, logos, icons, covers, portraits, store descriptions/genres, and news snapshots. Successfully cached content is reused after restart rather than silently replaced when Steam changes it.
- A **Refresh game cache** button on each game's detail page to clear that game's saved presentation data and request the latest content. Ownership, installation, download, and launch decisions remain live service data.
- Mouse, USB keyboard, and controller input; desktop/Start/taskbar shortcuts; favorites and library search; download controls and per-game controller settings; Paint, Notepad, and a system-browser launcher.
- The window-shadow fix that the owner confirmed made Vapor smooth: hidden window pixels are no longer blended 18 times per frame. Further improvements cache glyph coverage, Recent Games ordering, shortcut labels, and download-view updates.
- Automatic complete **cs2-019.1** runtime selection for Steam app 730, including the tested derived `ntdll.prx`. Other games retain their configured/base runtime.
- Unlocked default profiles for Skyrim Special Edition, Terraria, Brotato, and Counter-Strike: Source. Profiles do not establish gameplay compatibility, and saved player settings retain precedence.

## Verification

The combined installed package has **1,856 files**. Application/runtime contents, applicable file modes, and the final manifest were verified against pinned hashes. The app reopened successfully and its Steam startup and input initialization passed. Tests cover cache persistence/reset, pixel-identical text/shadows, library discovery, input, downloads, save-preserving migration, and complete CS2 runtime first-install/reinstall paths.

Owner gameplay reports are separate from host tests and startup checks. Local rendering benchmarks are **not PS5 FPS measurements**. A fixture reinstall is not a claim that the owner's entire CS2 download was repeated. See [current verification](docs/current-status.md) and the [changelog](CHANGELOG.md).

## Screenshots

Existing PS5 captures show the desktop and Vapor interface:

![WinBrew 11 desktop and context menu](docs/screenshots/desktop.jpg)

![Vapor game library](docs/screenshots/vapor-library.jpg)

![Vapor download manager](docs/screenshots/vapor-downloads.jpg)

## Credits and history

WinBrew 11 uses the original Playden/prospero-win work and the components identified in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This project is GPL-3.0-or-later; runtime components retain their own licenses and corresponding-source obligations. No commercial game files, Steam accounts, or saves are included.

Earlier prototypes and acceptance notes remain in the [historical desktop archive](docs/history/desktop-development.md). PPSA77381 packages and experimental commands in that archive are not the current installation path.
