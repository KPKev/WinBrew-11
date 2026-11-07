# Historical desktop development notes

Archived November 7, 2026. This document records earlier development stages, not current setup or compatibility. Statements below about PPSA77381, missing USB input, or unfinished Steam integration describe old builds. Start with the [current README](../../README.md), [compatibility](../compatibility.md), and [setup guide](../getting-started.md).

# WinBrew 11

A Windows 11-inspired desktop for native PS5 homebrew, created by **KPKev**, developed with AI assistance. The application is **WinBrew 11**, title **PPSA77381**, version **01.016.000**; **Vapor** is its game launcher and store section. The icon retains the four-pane concept with crisp geometry, restrained depth, a richer blue background and clear **WinBrew 11** lettering. Every in-app pane mark is derived from the icon master by `tools/make_brand.py`; About displays the complete app icon.

The desktop uses PS5 VideoOut and controller services. Supported Windows games use Playden's Wine-based PS5 compatibility runtime. Linux is a PC build environment through WSL, not the console runtime. WinBrew 11 is an independent homebrew desktop, not Microsoft Windows, an official Wine release, or a Playden release. Internal `wine11` storage and compatibility identifiers remain unchanged.

## Delivery

- Console folder: `/data/homebrew/PPSA77381`.
- PS5 package: `dist/WinBrew11-M1-PPSA77381.zip`.
- Corresponding source: `dist/WinBrew11-M1-source.zip`.
- Windows preview: run `Preview.cmd` to open `build/WinBrew11-Preview.exe`.
- Private native log: `/download0/wine11.log`.
- Launch targets: full main Quake II RTX `FAKE78003` and Playden `FAKE77031`.

The installed application currently uses its former Wine 11 display name; the WinBrew 11 package and console rebrand have not been deployed. The retired registration and source are absent. The owner previously confirmed a single Wine 11 tile with the former icon and name. `evidence/wine11-replacement.json` records registration, source and actual metadata-path icon verification.

The owner confirmed the corrected desktop opens and works and its game handoff reaches the menu and gameplay. A black world with visible HUD was restored by changing the game's video settings; the owner suspected a resolution mismatch. This project did not change the game configuration or graphics to resolve it. The home-screen icon and name were accepted. Desktop update 2 is installed and read-back verified; the owner confirmed the new desktop and Settings work on PS5. Browser hardware acceptance is pending. Consult `evidence/desktop-update-build.json` for preparation status and `evidence/desktop-update.json` after deployment.

## Interface and controls

Desktop update 2 adds an original blue bloom wallpaper, smaller 36px desktop shortcuts (46px optional), a compact 54px taskbar, denser Start/search/All apps, a game library, movable windows and a styled Settings window. System, Personalization, Taskbar, Accessibility and About pages include working light/dark modes, accent colors, three backgrounds, small/medium icons, left/center taskbar alignment, desktop icon visibility, 12/24-hour clock and pointer size. Preferences save across restarts in the app's private `/download0/desktop-settings.ini`. On PC they use `build/desktop-settings.ini`. The calendar is a date viewer; File Explorer is the installed-app library. This remains a single foreground desktop window model, not full Windows parity.

| Input | Action |
| --- | --- |
| Mouse on PC / left stick on PS5 | Pointer |
| D-pad / arrows / Tab / Shift+Tab | Select controls |
| Cross / Enter | Open selected item |
| Circle / Escape | Close the current flyout/window |
| Options / F1 | Toggle Start |
| Triangle / F2 | Game library |
| Keyboard typing / Backspace on PC | Search apps / erase |

Desktop icons use double-click with a mouse. PS5 input currently uses the controller; USB keyboard/mouse are not implemented. CPU rendering has not been benchmarked on hardware and must not be described as 60 FPS. The Windows preview uses the shared renderer and reports that PS5 launches require the console.

## Web browser

Web has a Chrome-inspired new-tab window, URL/search field, Google/Wikipedia/YouTube shortcuts and a controller-operated address keyboard. On PS5 it calls the native `sceSystemServiceLaunchWebBrowser` API; actual web content runs in the PS5 system browser, with its compatibility limits. Chrome and Chromium are not bundled, installed or verified. On the Windows preview the same action opens the PC's configured default browser. Text queries are URL-encoded for Google; only HTTP/HTTPS addresses are allowed. Browser error codes are shown when the platform rejects a launch. This is not an embedded Chrome engine or a claimed Chrome port.

## Game preservation

Desktop update 3 repairs the private preferences path after filesystem elevation. The app anchors its original download directory, then matches its device and inode against the sandbox-relative and global paths before binding saves and diagnostics. Saves flush and sync, atomically replace the preference file, and verify its exact bytes before displaying success. Failed saves explicitly say that the choice lasts only for the session. Version 1 preferences migrate to version 2 with pointer size, speed, edge margin and desktop audio preferences; these are separate from game settings. Hardware restart acceptance is recorded in the deployment evidence.

Mouse & pointer has three complete outlined arrow sizes, controller speed from 1 to 10, and optional 16/32px edge margins. Pointer bounds include its full shape, and stick motion uses elapsed time with a deadzone and fine-motion curve. The tray has live local network/IP status, a desktop-window flyout, and Wine 11 sound mute/volume/test controls when its audio port is available. Game/TV volume remains under PS5 sound controls. The browser declaration and call now include the SDK's second options argument; the earlier one-argument call failed the owner's page-opening test. Browser page acceptance of the repaired build is pending.

Desktop update 4 replaces the hand-drawn placeholder icons with detailed transparent system artwork, authentic Playden and Quake II RTX identification art, the browser logo and Microsoft Fluent system glyphs. `tools/make_icon_atlas.py` filters each icon into its actual display sizes in premultiplied alpha. The 3,753,600-byte font/icon atlas is provided in fixed BSS storage on PS5; no large title-heap allocation or runtime PNG/SVG decoder was introduced. Sources, generation prompts and provenance are retained under `art/icons`. About now shows **Created by KPKev** prominently and **Developed with AI assistance.** as a smaller secondary line.

## Playden engine integration direction

The intended product is KPKev's Wine 11 desktop as the entire user interface, with Playden/prospero-win providing the game services beneath it. This is broader than the current handoff to the separate Playden title. The existing Playden source is already split into reusable libraries in `cmake/PlaydenCore.cmake`, `SteamBackend.cmake` and `ProsperoRuntime.cmake`; there is no need to copy its presentation screens into Wine 11.

The inspected reference is the owner's separate `Playden/work/playden-source`, HEAD `9f5a7cbebeaf07fa3ea5324dce5ad8f374ca0aec`. It was read only. Candidate boundaries are:

| Wine 11 experience | Existing service boundary to adapt |
| --- | --- |
| Games library and account sign-in | `LibrarySource` / `LibraryService`, `playden_steam_auth`, `playden_steam_library` |
| Downloads window and installation progress | `installs::InstallService`, queued install events and volume records |
| Per-game Properties | runtime game-settings records and the existing publication/ownership transaction |
| Play, running state and exit status | `runtime::LaunchService`, `RuntimeHost`, title activity and the native coordinator |
| Wine execution and graphics | verified prospero-win runtime kit and its existing per-game bottles/profiles |

The first integration increment should pin a reviewed source snapshot and wrap read-only installed-game inventory behind a Wine 11 adapter. Next, compile and validate the existing preparation/launch services behind that adapter, then connect account/library/download events to new desktop windows. Preserve its worker ownership, cancellation, revision checks, cloud/save preflight, verified-kit admission and title-publication transactions. UI code should submit requests and render events; it should not directly rewrite Playden's private stores or duplicate its launch/session rules. This small native renderer currently uses a different minimal C++ runtime, so the service dependency and threading/toolchain integration must be validated before linking the full backend.

Do not operate two writers against the same Playden state or move existing game bottles as an incidental part of UI work. Start with a separate Wine 11 adapter state and explicitly reviewed adoption of existing installations. Keep GPL/LGPL/MIT source attribution and corresponding source with reused components; KPKev's product/design credit does not remove contributor notices. Full backend integration has not been implemented by this desktop repair.

## Existing games

The launcher writes no Quake profiles, executable, PAKs, saves, controller settings or material overrides. It does not bundle or reinstall the game or runtime kit. Full-game PAK sizes were checked before installation: 183997730, 12992754 and 45055 bytes. The existing autoexec still requests `ray_tracing_api query` and the FPS display.

The prepared game title accepts a normal home-screen style launch without a new Playden session argument. Root-proof/provider failures are reported. API acceptance is distinct from visible game acceptance. After closing a game, use the ordinary PS5 home screen to reopen Wine 11; automatic return is not implemented.

## Build and verification

Pinned inputs are in `dependencies.json` and the source archive. A fresh checkout can fetch them with `python tools/bootstrap.py`. Asset generation uses Pillow, NumPy and resvg-py. `tools/icon_design.py` generates the polished bitmap and an editable SVG layout master; `tools/make_assets.py` invokes it with the current Wine 11 label.

Windows with Visual Studio 2022 C++ tools:

```powershell
cmd /c tools\build-preview.cmd
```

WSL/Ubuntu with GCC C++ headers, Clang/LLVM 18, lld 18, Python, Make, Ninja, wget, curl, tar and unzip:

```bash
bash tools/test.sh
bash tools/build-ps5.sh
```

The builder stages the app under `vendor/ps5-native-app-boilerplate/wine11`. SDK digests, the deterministic clean-room libc checksum, exact-title helper manifest and protocol checks remain enforced. The helper uses Playden's pinned Lapy source and SDK v0.42 for PPSA77381.

Shared UI and mocked handoff tests pass. A storage test verifies zero array allocations for PS5-supplied buffers, correct ownership and invalid-storage rejection; it also passes ASan/UBSan. Native conversion/FSELF integrity, package hashes and unchanged-logo checks pass. The original title's corrected desktop and handoff were exercised on hardware. Desktop update 2 adds persistence, controller keyboard, spatial navigation and browser URL tests, including ASan/UBSan coverage. Hardware acceptance of the new browser path is pending.

### Startup lesson

The original title crashed at RIP `0x4001c9`, returning to `0x4028c4`. The exact ELF identifies an intentional allocation-failure UD2 in `operator new[]`, following the second 8294400-byte screen-buffer allocation. The generic privileged-instruction label did not establish CPU incompatibility. The correction supplies 17620992 bytes of BSS-backed frame/background/font storage on PS5 and guards borrowed ownership; host fallback allocations are checked. Historical evidence is preserved. `tools/history/update_buffer_fix.py.txt` is archival, not a current migration command.

## Console operations

The working PS5 address is `192.168.7.26` on Wi-Fi. zftpd uses 8888; Web File Manager uses 8889. The wired `.33` connection had live services but remained unreachable from the PC. Its cause was not established, and no router/firewall settings were changed.

`python tools/update_desktop.py` performs a read-only comparison of the current Wine 11 package. With `--apply`, it requires the title to be closed, backs up every changed resource, verifies staging by SHA-256, promotes each file by rename with rollback, and publishes the manifest last. The accepted icon, title ID, runtime and exact-title root helper are protected. It never uninstalls the title or edits game files.

`python tools/install_console.py` validates the local package without writing. `--host <IP> --port 8889 --install` installs only the new title, refuses an existing destination, checks the main-game PAK sizes, verifies every resource by SHA-256 read-back and publishes metadata last. `--resume` requires an exact unpublished staging tree.

The owner authorized complete removal of WrappedWine PPSA77380. Its 18-file source was backed up, verified and removed from `/data/homebrew` before the final normal UI deletion, preventing ShadowMountPlus from re-registering it every scan. Wine 11's source registration was temporarily held while both stale tiles were deleted. Their absence was verified from a read-only app.db snapshot; the unregistered Wine 11 source and old icon metadata were cleared with backups, then only the current Wine 11 was installed. The app database's actual `metaDataPath`, `/user/app/PPSA77381/sce_sys`, contains the verified polished icon, as do the source and auxiliary appmeta cache. Do not assume appmeta alone is the icon used by the shell.

The earlier native uninstall call through zftpd stalled that server. A normal SIGKILL through Payload Manager returned success but PID96 did not exit; no duplicate server was started. zftpd remains unresponsive, while Web File Manager on8889 and the other payloads remain available. No console reboot was performed. This service limitation is recorded separately from the completed app replacement. Do not blindly resend the uninstall, kill system processes or edit app.db to resolve it.

GPL-3.0-or-later. See LICENSE and THIRD_PARTY_NOTICES.md.

Desktop update 5 adds a shared text editor for app search and web addresses, a theme-aware interactive touch keyboard for controller entry, and native USB keyboard/mouse polling alongside the controller. Mouse activation does not force the keyboard open; physical typing remains available while it is visible. US keyboard text, Shift/Caps, caret movement, selection, backspace/delete, Ctrl+L and Ctrl+A are supported. Input & typing shows device connection state and persists USB mouse speed in preference schema 3, migrating earlier preferences. USB input is scoped to Wine 11; Sony's separately launched browser owns its own input and rendering. No Chromium engine or system-browser input bridge is included. Browser launch call duration is logged without URL/query/keystroke content; webpage loading latency remains unverified.

Web shortcuts, game-library cards and single-toggle settings rows use the entire card as their hit target, including their padding and artwork. Mixed input, stationary mouse focus, short mouse clicks, repeat timing, editing bounds and schema migration are covered by `tests/input_test.cpp`. Native hardware verification is pending installation and owner testing; the desktop test results do not prove HID works within Sony's browser.


Desktop update 6 is prepared for another console test. Update 5's whole-card hit surfaces were accepted on hardware, while both USB devices failed to open and website requests showed a blank page. The USB backend now resolves exact exports from the title's returned module list when internal sysmodule IDs do not yield a handle, with a standard library-loader fallback, and opens devices before filesystem elevation. A host mock reproduces the failed internal-ID lookup and verifies device lifecycle. Browser launch now prefers a preinitialized native WebBrowserDialog lifecycle (ABI checked against pinned EVO source), pausing Wine 11 input reads while the modal browser owns input and reopening devices when it closes. The legacy system-service launch is retained only if the dialog backend is unavailable. Neither path is Chromium. Settings > Network > Test browsing opens a simple HTTP page to distinguish opening from site-specific loading. This version has built and passed host checks; its browser page loading and USB hardware behavior remain unverified until console testing.


Desktop update 7 replaces the ineffective runtime-symbol discovery path with native linked imports. A bounded console probe reproduced successful module loading followed by `sceKernelDlsym` returning `0x80020003` for both readable symbols and their NIDs. Update 6 therefore never reached USB initialization and continued using the legacy browser fallback. The new build extends the link catalog in a separate `build/native-imports` directory for Mouse, CommonDialog and WebBrowserDialog; Keyboard uses its original SDK catalog. The generated stubs describe imports only and are never packaged. The upstream builder and SDK remain unchanged. Native lifecycle mocks and package integrity checks pass; real USB input, managed browser startup and YouTube navigation still need console testing.


Hardware feedback for update 7 confirms USB pointer movement and keyboard typing, and both Google and YouTube loading. Native logs confirm all linked input/browser exports, successful device opens, connection state and browser-dialog startup. USB left-click works on the webpages but was reported broken inside the desktop.

Desktop update 8 removes cross-poll timestamp rejection from Wine 11's queued mouse decoder. `sceMouseRead` reports are consumed as events; repeated timestamps do not identify duplicate events. Button edges still come from button-state transitions, so a held button does not activate repeatedly. A focused regression reproduces a lost stationary click with update 7's decoder and opens Settings with update 8. The input suite also checks equal/zero timestamp down/up batches and drag release. Host and native builds pass; the desktop click repair awaits console installation and verification.


Desktop update 9 adds PS5 controller preferences and a live input test, anchored desktop right-click menu, and a read-only File Explorer that lists accessible directories and previews text. Controller speed, deadzone, stick selection, vertical inversion and confirm/back preference are saved with schema 4; prior preferences migrate. Explorer shows accessible Wine 11, USB, app and PS5 data folders and reports inaccessible paths; it does not yet copy, delete, rename or open arbitrary binary files. The PS5 browser dialog handles its own input and webpage right-click remains unresolved. Xbox support was deferred at the owner's request. Console verification is pending.

The owner requested a wholly Wine 11-branded game experience. Desktop and Start shortcuts now open a Wine 11 Game Library rather than the separate Playden UI; the current library exposes the known Quake II RTX launch. Steam authentication, discovery, downloads and cloud services have not yet been connected to this library. Playden remains separately installed as the existing backend/reference, with contributor notices retained.


### Desktop update 10

Explorer uses the PS5 kernel directory API for folder listings, distinguishes empty folders from open/read errors, highlights the chosen location and supports bounded Up navigation. Location cards and sidebar navigation have pointer tests; files can be browsed and text previewed where the console grants access. USB and PS5 data access depend on mounted storage and title permissions. The actual console folder results remain to be verified by the owner. File operations such as copy, move, rename and delete are not implemented. Desktop window controls, taskbar tooltips and Game Library labeling were refined. The Wine 11 logo and game files remain unchanged.


### Desktop updates 11ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“13

Update 11 repaired the launch access check by moving elevation ahead of service initialization, changed Circle to a secondary/right click (subject to the saved button swap), and gave Game Library a distinct taskbar icon. The owner confirmed Quake launched and controller interaction worked, but USB stopped. Update 12 loads input and browser modules before elevation and initializes services after it; the owner confirmed Quake, USB mouse, USB keyboard and Google all work. Browser page loading remains intermittently slow or times out inside the PS5 system browser.

Update 13 is installed and started on the console; editor interaction is still awaiting owner verification: Explorer opens supported text files in a Notepad-style editor and PNG/JPEG/BMP images in Wine 11 Paint. Notepad accepts ASCII documents up to 64 KiB, preserves original newline style and BOM, checks for external changes, and saves through a verified temporary replacement; other encodings and oversized files are refused. Paint supports images up to 1024 x 768, brush, eraser, palette and Undo/Redo; it saves new PNG copies, never overwriting the source. Both warn about unsaved edits. The read-only game inventory adapter references Playden install records; generic launches, Steam sign-in, downloads and cloud services are not connected. The new image decoder adds about 14 MiB of fixed storage, so startup and file behavior require physical-console testing.

Quitting Quake currently returns to PS5 Home. Wine 11 is terminated during game handoff; automatic return requires a separate verified game-wrapper change and has not been implemented.


### Desktop update 14

On the PS5, the owner confirmed that a 3840 x 2160 USB screenshot opens in Paint, the edited PNG saves alongside its source, and Explorer Properties, Copy and Paste work. Paint preserves full resolution and saves an independent PNG; a 4K copy can take around 30 seconds on the USB drive and the UI currently blocks during that save. Paint currently provides one Undo level; multi-step Undo and responsive saves are in development. Explorer Cut and directory copying remain unavailable.

The PS5 browser reported that it could not resolve accounts.google.com. The owner confirmed HTTP and HTTPS example.com load quickly and reports Google sign-in sometimes disappears during and between dialog sessions. PS5 primary DNS is the router at 192.168.7.1 with no secondary. The platform browser does not expose a verified persistent profile to Wine 11; the underlying DNS and authentication causes remain under investigation.


### Desktop update 15

The owner confirmed multi-step Paint Undo/Redo and responsive save progress on a 4K screenshot, and confirmed the Recent websites list is populated after opening Google. Paint keeps up to 20 actions in a bounded 32 MiB history; extremely large edits may evict older actions. A USB filesystem flush may still briefly block one system call. Recent websites keeps only site origins explicitly opened by Wine 11; it cannot collect internal PS5 browser navigation, cookies, or preserve sign-in.

## Steam-style library work in progress

The Wine 11 game library now uses a dark Steam-inspired Library and Downloads layout, with selectable detected installations and page controls. Quake II RTX retains its existing launch action. Other detected installations display their recorded state; their launch, Steam account, online library and downloads remain unconnected. The Playden source is pinned under `vendor/playden-source` by bootstrap and included in the corresponding source archive for the planned service integration. No Steam credentials or game data are migrated by this increment. The Windows preview and host tests do not establish console behavior.

The experimental Steam service coordinator in `src/steam_integration` uses Playden�s actual asynchronous authentication and library services. Its host test uses fake credentials, storage and network boundaries; it is not yet linked to the Wine 11 UI or the PS5 executable and does not enable account sign-in.

`bash tools/probe-steam-compiler.sh` compiles the Playden authentication, library, install, and launch workers and the Wine 11 coordinator with the PS5 SDK C++ compiler. The existing Wine 11 native build uses `-fno-exceptions`; the Playden workers contain exception handlers and cannot be compiled with that option. A successful compile-only probe does not prove a compatible runtime or linked title. `tools/prepare-steam-network.sh` builds the pinned PS5 networking inputs and verifies their archives, headers, TLS configuration and CA bundle against Playden�s lock.

The first Steam network build used the desktop project�s SDK v0.42 and Ubuntu LLVM 18.1.3; all six resulting library hashes differ from Playden�s lock, which calls for SDK v0.43 and LLVM 18.1.8. Those outputs are unverified and must not be linked. The preparation wrapper checks the SDK provenance and compiler version before rebuilding, then checks the resulting hashes.

### Desktop update 16

Settings > About > Game return > Run check performs an explicit read-only observation using the installed prospero-win publication reader and a shared lock on the existing publication. It reports fixed messages only, never changes game files, permissions or ownership, and never runs during startup or drawing. Its portable tests verify one-shot dispatch, launch/browser priority, missing-lock refusal, ownership/metadata refusal and unchanged host-fixture contents/modes/inodes. Native admission, actual owner context and lock-gateway behavior still need hardware testing. This check does not install the separate automatic-return wrapper or authorize an upgrade; full kit and publisher admission remain required.

A subsequent build used the locked SDK v0.43 and an isolated apt.llvm.org LLVM 18.1.8, but the resulting PS5 TLS archive hashes still differ from Playden�s lock. The exact upstream binary provenance is unresolved; the outputs remain unverified. A deliberately opt-in link diagnostic connects the real authentication and library clients to the coordinator using those unverified archives. It is a build-only result, must not be run or deployed, and does not replace the normal hash gate. All staged Wine 11 native sources compile individually with the Playden SDK and C++ headers; that check is not a linked app or hardware test. `src/steam_integration/runtime.cpp` composes Playden�s real account, shared CM, private session, library and cache services, but it is not yet connected to the desktop or shipped.

The separate `Runtime` composition and its real Playden account, entitlement, session and library dependencies linked in an opt-in PS5 diagnostic using the locally built, hash-mismatched networking archives. It was not executed, is not a Wine 11 title build, and is not eligible for deployment. The strict link path continues to reject those archives.

The experimental account adapter now connects the Steam-style desktop panel to Playden�s asynchronous account and library services. A simulated end-to-end host test exercises mobile QR approval, a paged account library, refresh and sign-out. The console and normal PC preview still do not instantiate this adapter: real Steam sign-in, account games, installation and launching are not yet available in those builds. The PS5 trust-archive hash gate remains in force.

Experimental download integration uses Playden�s install service behind a dedicated command worker. When an exclusive private `wine11-games` directory and publisher lock are supplied at startup, the runtime can connect Playden�s real Steam install runner; restored jobs and removals are guarded before the install worker starts. The host tests use temporary storage and a simulated runner, not a Steam download. The UI blocks switching or signing out of an active account while jobs exist, while allowing an initial or retried sign-in for work restored after restart; existing Playden records remain distinct from new Wine 11 installs. This integration is not enabled or linked into the released console build; the PS5 network archive hash checks and hardware verification remain outstanding.

The Downloads view renders queued, running, paused, failed and recently completed work, with pause/resume, explicit cancellation and clearing completed rows. A host integration test drives these through Playden�s actual install queue and a simulated runner; it does not download a real Steam game. The startup storage guard checks an exclusive publisher lease and canonical Steam game paths before transfer or removal. The underlying Playden registry still uses path-based operations and assumes a stable managed namespace; the entire console integration remains gated and unverified.

The locally rebuilt PS5 Steam networking archives remain different from Playden�s pinned binary hashes. An isolated ELF-loader diagnostic on the console subsequently verified the exact rebuilt archives could initialize their threading mutex, connect over HTTPS to `example.com` and Steam�s public server-info endpoint, and reject a valid but unrelated certificate authority. It did not use Steam credentials, write console files, modify the installed app, or test Playden�s full HTTP client, Steam sign-in, CDN downloads, Cloud saves or game launch. The isolated result does not change the upstream hash gate or establish release readiness; see `evidence/steam-network-console-tls.json`.

An experimental prepared-game launch bridge now routes a Play request through Playden�s RuntimeHost, checks that the account, game identity and installed path match, requires a completed cloud-save gate, and invokes handoff only on the application thread. A simulated host test checks refusal and native failure handling. The app does not yet configure a production RuntimeHost or cloud gate, prepare titles through this interface, or launch these games on the console.

Subsequent isolated console probes exercised Playden's HTTP client and obtained a real Steam QR authentication challenge with the tested rebuilt networking archives. The challenge was canceled without signing in. See `evidence/playden-http-console-probe.json` and `evidence/playden-auth-qr-console-probe.json`. These probes do not establish that a real account, library, download, Cloud sync or game launch works in the app.

The experimental native bridge can construct a real Playden game host and attach a Cloud preflight, but it is not yet wired into the shipped app. A separate Wine 11 title-list adapter and helper are being prepared so that game publication does not replace Playden's list or kit. The rebuilt helper has not been run on the console, and the separate game kit has not yet been assembled or admitted. The normal Playden hash checks remain unchanged.

`tools/derive-steam-runtime-kit.py` prepares a separate local kit from a complete, independently validated copy of the original Playden kit. It checks the Wine 11 helper and protocol, updates the dedicated title rule and Wine Mono path, regenerates the inventory, and runs the Playden validator with an explicit local helper pin. A fixture test covers the original kit's preservation, output validation, tampering, and overwrite refusal. This tool has not been run on the complete console kit and does not install or qualify it.

`tools/copy-playden-kit-readonly.py` can copy the observed console kit into a new local POSIX directory, verifying each file against the observed manifest before writing the manifest last. It does not write to the console or replace the complete upstream kit validator. It has passed simulated file, path, permission and overwrite tests; the console file service was unavailable, so the full copy has not started.

`tools/stage-steam-runtime-assets.py` stages Playden's pinned presets, recipes, input profiles and Steam compatibility files in a separate local build directory, retaining their included notices. The compatibility DLLs are Playden's gbe_fork-based files; Playden's preparation code can replace a game's Steam API DLL with them while retaining the original. The staged presets and recipes were loaded by Playden's real settings parser. This is not an installed console package.

The original Playden 0.13 release ZIP was downloaded locally and matched GitHub's published SHA-256; its kit manifest also matched the manifest previously observed on the console. Every kit file matched its manifest. The current offline validator accepted it for local testing with its explicit development build-path exception: four compiled files contain embedded host build paths. That exception is a release blocker and does not qualify the kit for redistribution or console use. A separate Wine 11 kit is being derived locally; no console kit was changed.


For a fresh checkout, `python tools/fetch-steam-ca.py` fetches the pinned public CA bundle into a new `build/steam-inputs/cacert.pem` after verifying its exact size and digest. Then `python tools/stage-steam-runtime-assets.py build/steam-runtime-assets --ca-bundle build/steam-inputs/cacert.pem` stages those assets into a new directory. These steps do not build or qualify the PS5 networking libraries, install Vapor, or establish game-launch compatibility.

The local game-kit preparation tools also require the pinned validator checkout. `python tools/fetch-prospero-validator.py` prepares or verifies that checkout without replacing an existing folder; a fresh network clone has been verified; the complete PS5 kit still requires separate verification.
