# WinBrew 11 third-party notices

WinBrew 11 is GPL-3.0-or-later. The source and build scripts accompany the local project. This is an independent homebrew desktop inspired by Windows 11, not Microsoft Windows. No Microsoft Windows binaries, Segoe fonts, Microsoft wallpapers, or Quake executables, PAKs, textures or saves are included. Game-identification icons are credited below.

- **PS5 Native App Boilerplate**, BlackBearReloaded, GPL-3.0-or-later. Mirror: https://github.com/leolobato/ps5-native-app-boilerplate at `38c68b4645ab6a1470c7598df1a3fc10d3e0cae0`. The native toolchain, clean-room libc, VideoOut implementation and Lapy client are reused. `tools/stage_build.py` adds a linear framebuffer blit to a build-local copy of the renderer and changes diagnostic labels. Original source remains under `vendor/ps5-native-app-boilerplate`; its notices and complete license are retained.
- **Playden PS5**, Leo Lobato and contributors, GPL-3.0-or-later: https://github.com/leolobato/playden-ps5. Launch parameter ABI and provider sequencing in `src/title_launch.hpp` and `src/ps5.cpp` derive from `src/platform/ps5/title_runtime.*` and `src/homebrew/ps5.cpp` in the user's local Playden source. Playden and its Wine runtime remain separately installed and are not bundled or modified.
- **ps5-homebrew-ui**, BlackBearReloaded, GPL-3.0-or-later: https://github.com/leolobato/ps5-homebrew-ui at `0a6fb4b851da86cd4e9512aae3abf1352a96086a`. The 120-byte controller sample ABI in `src/ps5.cpp` is based on its `src/platform/ps5/pad.cpp`; native desktop-audio declarations and constants were checked against its GPL `audio_out.cpp`. The UI library itself is not linked.
- **Selawik**, Copyright 2015 Microsoft Corporation, SIL Open Font License 1.1, reserved name Selawik. https://github.com/microsoft/Selawik release 1.01. The unmodified fonts produce our raster text atlas; `licenses/Selawik-OFL.txt` accompanies every package.
- **PS5-Lapy-JB-Daemon**, ArkSama / Team PHU and mpereiraesaa's fork, MIT. Wine 11 builds Playden's pinned fork https://github.com/leolobato/PS5-Lapy-JB-Daemon at `66ab4f10f9a6c6a5a42d05f1d165230c13ae2ab7`, using SDK v0.42, for exact title `PPSA77381`. Its shared protocol is LGPL-2.1-or-later; see `licenses/LGPL-2.1.txt`. The ps5log header comes from Playden's pinned prospero-win source at `e709a0b72361c6c8e16c80ff93a3705a5e5421b7`, SHA-256 `05373177e2c1cd348848014139646e8b35306da673ff5c0a9089c1247eda394f`, GPL-3.0-or-later. The MIT notice and generated helper provenance accompany the app. No generic or wildcard root helper is used. This exact new title/helper combination requires hardware validation.
- The native builder uses its digest-pinned public PS5 payload SDK and zlib 1.3.2; see the bundled boilerplate notices and source lock. These are build inputs. No proprietary Sony SDK implementation is bundled.

The original blue bloom wallpaper was generated for this project with OpenAI image generation. Its source-size image is retained in `art/blue-bloom-source.png`; `tools/make_assets.py` prepares the display assets and original vector icons. The browser launch API declaration was cross-checked against itsPLK/ps5-payload-manager `e4e72dc`, `src/ps5_launcher.c`; no implementation from that project is bundled. Chrome itself is not included. Windows, Chrome, PlayStation and Quake are trademarks of their respective owners.

- **Microsoft Fluent UI System Icons**, Microsoft Corporation and contributors, MIT: https://github.com/microsoft/fluentui-system-icons. Search, controller, WiFi, speaker and network-adapter SVGs are rasterized into the shared atlas; `licenses/Fluent-System-Icons-MIT.txt` is bundled.
- **Simple Icons**, CC0-1.0: https://github.com/simple-icons/simple-icons. Wikipedia and YouTube identification marks are rasterized from its SVGs; `licenses/Simple-Icons-CC0.txt` is bundled.
- **Google Chrome identification logo**: https://www.google.com/chrome/static/images/chrome-logo-m100.svg. Used for the Chrome-style Web launcher; Google Chrome itself is not bundled or claimed to be installed.
- **Playden identification icon**: read-only copy of the owner's installed Playden title artwork. Playden contributor attribution and GPL notice above remain applicable.
- **Quake II RTX identification icon**: largest frame of Q2RTX source `src/windows/res/q2rtx.ico`. NVIDIA Corporation and Quake II RTX contributors; see the retained `licenses/Q2RTX-LICENSE.txt`. No game executable or data package is bundled.
- New PC, recycle-bin, folder and settings artwork was generated for Wine 11 with the built-in OpenAI image generation tool. Exact prompts, source paths and hashes are in `art/icons/provenance.json`; transparency is preserved.

- **prospero-win USB input backend**, prospero-win contributors, LGPL-2.1-or-later. `src/ps5_hid.cpp` adapts module loading, device opening and report ABI from `native/pw_hid_ps5.c` in the owner's local prospero-win source. Portable decoding and desktop text editing are implemented in `src/hid_input.cpp` and `src/text_input.cpp`; no keystroke payloads are logged. Source and LGPL notice accompany the package.

- **EVO-PLAYER-PS5**, sainsaji and contributors, GPL-3.0-or-later: https://github.com/sainsaji/EVO-PLAYER-PS5 at `9ee9a5420c73b5be448610e5f0a90a29a75592f3`. The browser-dialog ABI layout and pre-elevation initialization lifecycle in `src/ps5_browser.cpp` were checked against `projects/evoplayer/src/evo_webui.c` and `docs/research/web-browser-dialog.md`. Wine 11 implements its own small lifecycle adapter and uses the platform browser directly; no reverse proxy, page injection or certificate-verification change is included.

Native import catalogs in `src/native_imports.json` contain public API names only. `tools/prepare_native_imports.py` creates link-only stubs in a separate build directory and a build-local upstream driver; it does not modify the pinned SDK. Those stubs are never packaged or executed on the console. Keyboard uses the SDK's existing catalog. Runtime calls resolve to the PS5's system libraries. Following console evidence that runtime symbol lookup fails, update 7 uses linked imports for both USB and browser APIs.

- **stb_image 2.30**, Sean Barrett and contributors, MIT: https://github.com/nothings/stb, revision `003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5`. The image decoder and its license accompany Wine 11 in `src/stb_image.h` and `licenses/stb-MIT.txt`; local provenance is recorded in `licenses/stb-SOURCE.json`.
- **stb_image_write**, Sean Barrett and contributors, MIT: the same stb revision provides the PNG encoder used for PS5 game icons. Source is `src/stb_image_write.h`; provenance is recorded in `licenses/stb-image-write-SOURCE.json`, and the license is `licenses/stb-MIT.txt`.
- The read-only game inventory adapter references Playden PS5 install registry and marker formats at revision `9f5a7cbebeaf07fa3ea5324dce5ad0aec`, GPL-3.0-or-later. It does not run Playden installation workers or modify game state.

The read-only owner diagnostic reuses prospero-win publication admission from installed revision ce289425 (LGPL-2.1-or-later), with its pinned Mbed TLS publication SHA-256 provider. Corresponding source and provenance are retained under src/owner_admission; the original provider license accompanies this preview. It performs no recovery or game-state writes.

- **QR Code generator**, Project Nayuki, MIT: `vendor/playden-source/third_party/qrcodegen`; license is included as `licenses/qrcodegen-MIT.txt`.
# Steam header artwork

`assets/steam-header-logo.png` is the unmodified public Steam header image from
https://store.cloudflare.steamstatic.com/public/shared/images/responsive/header_logo.png.
The Steam name and logo belong to Valve Corporation; their inclusion does not
imply Valve endorsement of Vapor.

