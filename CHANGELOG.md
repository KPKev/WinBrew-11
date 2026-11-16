# Changelog

## November 16, 2026 — automatic Blender integration

- Vapor includes Steam software applications and refreshes the active account's library correctly.
- Normal Blender preparation installs/upgrades the pinned runtime and connects WinBrew Documents/Desktop automatically, without manual title renaming or console commands.
- Save As/Preferences use the main window; older projects retain the working interface instead of restoring unsupported secondary windows.
- Blender file-browser deletion moves items into the recoverable WinBrew Recycle Bin; shared-file permanent deletion also works in WinBrew.
- Notepad supports5MiB text files with heap-backed buffers and bounded history.
- Owner PS5 acceptance, guarded installation, native migration/refusal fixtures and host interoperability checks are documented in the [engineering report](docs/reports/blender-automatic-integration-20261116.md) and [verification receipt](docs/blender-shared-verification.json). Large scenes/render/export and other consoles remain outside the tested scope.

## November 14, 2026 — Recycle Bin fix

- Fixed a long-running PS5 Recycle Bin issue that prevented older deleted items from being collected and produced access errors.
- Deleted files now appear directly in the bin. Restore returns an item to its recorded original location, and items can be deleted individually or by confirming Empty Recycle Bin.
- Added a narrowly scoped helper for Recycle Bin operations, with checks that keep changed or inaccessible items instead of blindly deleting them. Older nested recovery entries are handled without exposing internal folders as the deleted file.
- I tested the installed update on my PS5: deleting, viewing, restoring and emptying the bin all worked. Separate PS5 fixture tests checked cancellation, recovery, restore and deletion while preserving a file outside the test bin.
- Added desktop drag selection/icon placement and controller Triangle for parent folder, Square for back. Existing game compatibility cautions still apply.

## November 10, 2026 — notepad-start-recycle-dev-v1 development update

- Redesigned Notepad with tabs, find/replace, undo/redo and improved save prompts; owner confirmed physical-keyboard typing on the preceding PS5 build.
- Unified mouse/controller desktop context actions, kept pinned Start and All Apps visible with context menus, and added pin/unpin controls for built-in app placements. Removed the legacy built-in Quake II shortcut/search launch; Vapor remains.
- Added a customizable display name, defaulting to WinBrew, while retaining the existing personal-folder path.
- Reworked Recycle Bin visibility, older-store collection, individual deletion, empty confirmation/cancellation and verified cross-drive recycling. Added bounded browsing and tests for large collections.
- Local UI and file-operation fixtures passed; isolated PS5-native file-operation checks passed. The final release executable has not been launched or visually checked on the PS5. Existing game compatibility caveats remain.

## November 7, 2026 — desktop-storage-dev-v1 development update

- Windows-style Explorer tabs, breadcrumb/address/search bars, left-aligned tree and details, drive cards, scrolling, sorting, multiple selection, views, preview, rename and drag/drop.
- Verified copy/cut/paste, recoverable deletion, confirmed Empty Recycle Bin, compact centered context menus and taskbar unpinning.
- Notepad Save As, editable save/close names, replacement confirmation and growing/wrapped dialogs. Paint and bounded CSV/XLSX workbook editing/preservation from the Office workstream.
- C: now maps the large console `/data` storage; duplicate D: hidden. Verified personal-folder/recovery migration to `/data/WinBrew/Users/Default` retains originals and keeps private account/settings state in place. Console Root exposes other accessible locations.
- Real mounted-device detection rejects empty 2 MB connection folders; USB insertion/removal and capacity refresh are owner-confirmed.
- Gold Vapor status text and pulsing Preparing/Starting Play feedback. Owner-confirmed Enshrouded startup after scoped preparation-state recovery; not a universal journal-reset feature.
- Corrected native PRX executable modes and replaced an unsupported managed-title storage import with the checked libkernel query. Owner-confirmed recovery from the Explorer crash.
- Installed/readback-verified final executable; owner confirmed large C:, Desktop files, Console Root and prior USB hotplug behavior. Host/native file, profile, editor and workbook checks pass.
- **Known regression:** owner now reports Skyrim Special Edition not working; another affected title remains unidentified. Historical compatibility is not a current-build guarantee.

See [the detailed multi-workstream report](docs/reports/development-20261107.md).


## November 7, 2026 — combined-blender-v1 release

- Blender: pinned installer/reinstall bundle, Vulkan defaults, independent cursor updates, mouse-wheel forwarding, loading-indicator fix and numbered sandbox discovery.
- Preserve CS2 offline compatibility; official online unsupported and not pursued.
- Kane's Wrath: first splash only. Splitgate: blocked at server authentication. Co-op/multiplayer testing deferred.
- Owner confirms the prior Blender unexpected exit was resolved by the cursor repair; large-scene/render/export coverage remains unverified.

## November 7, 2026 — combined desktop, Vapor, and CS2 update

- Persistent per-game artwork, store details, and news cache with a **Refresh game cache** action; automatic library detection remains enabled.
- Fixed expensive window-shadow rendering that caused mouse/controller dragging in Vapor. Owner confirmed smooth input after this fix.
- Reduced repeated text, Recent Games, shortcut, and download-view work. Added focused rendering/cache/input and publication regressions.
- Bundled and automatically selected the complete fixed `cs2-019.1` runtime for Steam app 730, including normal install/reinstall and save-preserving migration checks.
- Included default Skyrim Special Edition, Terraria, Brotato, and Counter-Strike: Source profiles, while preserving saved user settings. Profiles are not compatibility confirmations.
- Published complete application/base-kit downloads, corresponding runtime source, checksums, permission-preserving packaging, and a pinned file-mode repair helper.
- Installed and readback-verified the combined 1,856-file application; startup and input initialization passed. Further owner testing of the combined update is pending.
- Recorded owner reports: **Schedule I works; CS2 offline bots work**. LAN/local play and Internet tunneling are untested; official online authentication/matchmaking remains unsupported.
- Replaced obsolete current setup instructions with installation, compatibility, verification, and source-build guides. Preserved earlier development notes in the historical archive.

The published application remains version `01.016.000`, title `PPSA77382`. Documentation updates on `main` do not imply a rebuilt release binary.
