# WinBrew 11 — automatic Blender shared folders

**Release:** `blender-shared-files-v1` · November 16, 2026 · **Application:** PPSA77382, version `01.017.000`. Development release.

## Included fixes

- Vapor includes Steam software such as Blender when refreshing the active account's library. An account change refreshes ownership and application metadata.
- Blender gets the matched `019.3` runtime, portable launcher and scoped filesystem helper during normal preparation. Supported older runtime installations upgrade on Play; no title renaming or manual repair commands are required.
- Save As and Preferences open inside Blender's main window, avoiding the black secondary window.
- Blender Documents and Desktop connect directly to the same folders used by WinBrew Explorer. Existing private Wine folders are retained as `*.before-WinBrew` during migration. The `prospero` name in Wine paths is a virtual Windows user name: those Documents/Desktop shortcuts lead to WinBrew's actual storage, not a second saved copy.
- Shared Blender files can be sent to the Recycle Bin and permanently deleted through WinBrew’s normal confirmation flow. The release also routes Blender file-browser deletion to this recoverable format; the owner confirmed this works without errors or black windows.
- Opening a project preserves the current workspace instead of restoring saved secondary windows. Scene data and the original project file remain unchanged.
- Notepad accepts text files up to **5 MiB (5,242,880 bytes)**. Heap-backed buffers and bounded undo/redo history limit memory growth. Files above the limit are refused without replacing the open document.

## Automatic setup and scope

The app initializes a verified WinBrew profile, enrolls only prepared Blender titles in a dedicated root-owned access list, and grants the shared group access to Documents and Desktop. The helper validates paths and metadata before changing permissions. Unsupported entries stop preparation; ordinary migration failures roll back changes made during that attempt. File contents and ownership are preserved. Unrelated hidden metadata is preserved and symbolic links are not followed. Shared recycle stores and their recovery metadata are included. This is a shared-folder integration, not general access to every WinBrew folder.

Both launch routes use the managed Blender launcher. Existing installations should open Vapor and press Play once after updating so the runtime and access enrollment are refreshed before using the PS5 Home tile.

## Verification

The owner confirmed library visibility, visible Save As, Documents/Desktop saving and reopening, shared-file Recycle Bin deletion, and editing files above the former 1 MiB Notepad limit during this repair series. On the final corrective build, the owner confirmed Blender's file-browser recycle action works without black windows and the original `Untitled.blend` opens normally. Five installed Blender title hashes match the final runtime; the normal Vapor session confirms shared-group access and installation of the embedded compatibility policy. Host installer/upgrade tests, actual WinBrew restore-format interoperability and PS5 migration/rollback/recycle-permission fixtures passed. See `VERIFICATION.json` for exact scope and hashes.

This remains a development build. Large Blender scenes, rendering/exporting, long sessions, other consoles and every firmware are not certified. Earlier game compatibility reports remain historical; Skyrim Special Edition has an unresolved reported regression. No new online/co-op support is claimed.

## Install and source

The release assets are **WinBrew-11-Blender-Shared-Files-v1.zip**, **blender-runtime-019.3.zip**, **blender-runtime-corresponding-source.zip**, **WinBrew-11-Source.zip**, **SHA256SUMS.txt**, **INSTALL.md** and **fix-vapor-install-modes.py**. Follow the [installation guide](getting-started.md). Retain a correctly installed base kit; first installations also need the unchanged [Vapor-base-kit-0.13.zip](https://github.com/KPKev/WinBrew-11/releases/download/combined-blender-v1/Vapor-base-kit-0.13.zip).

The complete application includes the Blender runtime. The separate runtime ZIP is for source builds. No Blender application files, commercial games, Steam accounts, projects or saves are included. Matching runtime source and notices accompany the release. See the [engineering report](reports/blender-automatic-integration-20261116.md) for the complete repair history.
