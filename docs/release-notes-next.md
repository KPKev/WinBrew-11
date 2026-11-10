# WinBrew 11 / Vapor — Notepad, Start and Recycle Bin development update

**Release:** `notepad-start-recycle-dev-v1` · **Date:** November 10, 2026 · **Application:** PPSA77382.

WinBrew is an independent, Windows 11-inspired PS5 homebrew desktop. It does not install Microsoft Windows. Vapor and the existing CS2 and Blender runtime bundles remain included.

## What's new

- **Notepad:** refreshed interface, tabs, find and replace, undo/redo, Save and Save As, editable names and replacement confirmation. Unsaved edits are protected by prompts. The owner confirmed physical-keyboard typing on the preceding PS5 build.
- **Desktop and Start:** mouse right-click and controller Circle offer the same contextual actions, including cut, copy, paste and delete on desktop files. Start and All Apps keep the selected app visible behind its context menu. Built-in apps can be pinned or unpinned from the desktop, Start and taskbar; built-in All Apps entries remain available.
- **Personalization:** change the display name shown in Start. Its default is `WinBrew`. The actual personal folder remains at `C:\WinBrew\Users\Default`, avoiding moves or broken paths when the display name changes.
- **Recycle Bin:** deleted items are collected in the bin and can be removed individually. Empty asks for confirmation, with Cancel available. Older recovery locations are collected before emptying, including from connected volumes; copying from another drive is verified before the original is removed. Large bins can be browsed in batches; search and sorting apply to the current batch.
- The built-in Quake II shortcut and legacy search launch were removed. Vapor remains available; no game files are included or removed by this update.

## Verification and limits

The preceding executable was installed and readback-verified on the owner's PS5. The owner confirmed physical-keyboard typing. Local tests cover Notepad, Start and desktop context input, Recycle Bin confirmation/cancellation, individual deletion, legacy recovery, large bins and cross-drive transfers. An isolated PS5-native file-operation fixture passed with its own temporary files. The final release executable, including the last legacy-search correction, has not been launched or visually tested on the console. Tests do not guarantee every game or filesystem case.

This is an active development build. The owner reports Skyrim Special Edition is not working; another affected title is unidentified. Previous game compatibility reports are historical and online multiplayer is not established. See [compatibility](compatibility.md) and [current status](current-status.md).

## Downloads and update

Download **WinBrew-11-Notepad-Start-Recycle-Dev-v1.zip**, **SHA256SUMS.txt**, **INSTALL.md** and **fix-vapor-install-modes.py** from this release. For a first installation also download the unchanged [Vapor-base-kit-0.13.zip](https://github.com/KPKev/WinBrew-11/releases/download/combined-blender-v1/Vapor-base-kit-0.13.zip); existing correctly installed base kits can be retained.

Save work, close WinBrew and back up your app and personal files before updating. Follow the [installation guide](getting-started.md). The source archive and GitHub tag contain application source and tests; they are not a ready-to-run app. CS2 and Blender corresponding source archives remain available on the [combined release](https://github.com/KPKev/WinBrew-11/releases/tag/combined-blender-v1). No commercial game files, accounts or saves are included.
