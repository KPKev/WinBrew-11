# WinBrew 11 / Vapor v1.18.0

November 16, 2026. Development release for the owner's PS5 13.60 test environment. App PPSA77382, version 01.018.000.

## What's new

- Upgraded Vapor's integrated Playden backend to **0.11.0** and its matched base runtime kit to **0.28**. WinBrew's desktop, Steam library and scoped CS2/Blender runtimes are retained.
- **Backend Updates**, beside Library, checks when WinBrew starts and supports **Check now** whenever you want. A newer upstream Playden release is distinguished from a compatible Vapor update ready to install.
- **Update backend** downloads and verifies the matched app/runtime pair. A restart dialog finishes the staged update; the previous pair is retained for **Restore previous version**. A stock Playden archive is not installed over Vapor. New upstream revisions still need a compatible WinBrew package before they can be installed here.
- Small gold text and a progress bar above **Preparing** show the real setup step, elapsed time and percentage when available. Percentages describe the current step; unknown-duration work has an animated indicator, without a guessed ETA.
- Existing games automatically refresh their prepared title/runtime on Play when an older version is detected, preserving their bottle and scoped runtime choice.
- Fixed startup ordering so update confirmation and resumed downloads cannot block saved sign-in and library restoration. Preserved paused downloads and installed-game detection.
- Fixed library additions arriving after the initial refresh. Licensed desktop applications such as Blender appear normally; verified HTTPS metadata lookup and name caching are enabled.
- Retained the generic executable-path correction and metadata-based DX11 selection used during the Dungeon Defenders II investigation. Explicit user launch arguments are respected.

## Install this release

Download **WinBrew-11-v1.18.0.zip** and **Vapor-backend-kit-0.28.zip**, plus **SHA256SUMS.txt** and **fix-vapor-install-modes.py**. Both matched archives are required when moving from the earlier 0.13 base kit. The full app already includes the scoped CS2 and Blender runtimes.

Older Vapor versions do not have this updater: install v1.18.0 manually once using [the installation guide](getting-started.md). From v1.18.0 onward, compatible future releases can be installed from **Backend Updates**. Save your work and close games before restarting an update. Existing game, account, project and save folders are separate from the app/runtime update; keep backups when updating manually.

## Verification and limits

The final executable and kit were checked on the owner's PS5. The owner confirmed Steam/library visibility, Blender's **PLAY** button, the paused download queue and the **INSTALL** action for an intentionally uninstalled game. Native updater fixtures exercised staging, restart/apply and rollback. The real release archives also pass the production updater's download/stage/apply/confirmation/rollback flow in a disposable host fixture, including all app hashes and kit admission. See [the verification receipt](vapor-backend-verification.json).

This verifies startup, library controls and update transactions; it does not certify gameplay for every title or firmware. Dungeon Defenders II previously reached its login menu after the DX11 repair; online login and LAN/co-op remain unverified and are not promised by this release. Skyrim's previously reported regression remains unresolved. Prior Blender shared-file/recycle and CS2 offline results are historical and were not all repeated after this backend migration. The live public GitHub update download has not been exercised from the owner's existing same-version installation.

## Source and attribution

WinBrew/Vapor builds on **Playden by Leo Lobato and contributors**. The frontend, pinned source revision, deterministic integration patch and kit derivation tools are included in **WinBrew-11-Source.zip** and the repository. **Vapor-Playden-backend-source.zip** also supplies the derived backend source. The runtime kit carries upstream component licenses and source references; **BACKEND-SOURCES.md** records the derivation. Matching **cs2-runtime-corresponding-source.zip** and **blender-runtime-corresponding-source.zip** accompany the scoped runtime binaries. No commercial games, Blender application files, Steam accounts, projects or saves are bundled.
