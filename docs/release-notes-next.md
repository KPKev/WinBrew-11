# WinBrew 11 / Vapor — Explorer, Office and console-storage development update

**Release:** `desktop-storage-dev-v1` · **Date:** November 7, 2026 (America/Chicago) · **Application:** PPSA77382, system version 01.016.000.

> [!WARNING]
> This project is in active development. Games may unexpectedly stop working after changes, even if they worked earlier. **Skyrim Special Edition is currently reported down.** Another affected title has not been identified. Please check back often for updates and fixes; compatibility listings are not guarantees for the current build.

## Included

- Windows-style Explorer tabs, address/breadcrumb/search bars, left-aligned tree/details, drive cards, sorting/views/selection, preview, contexts and drag/drop.
- Verified copy/cut/paste, no-overwrite rename, recoverable Delete, confirmed Empty Recycle Bin, progress/cancel, compact centered menus and wrapped notices.
- Notepad Save As and editable save/close names with replacement confirmation; Paint and bounded CSV/XLSX editing with workbook-part preservation.
- Large console-backed C: (`/data`), personal folders at `C:\WinBrew\Users\Default`, verified first-start copying with retained originals, duplicate D: hidden, and Console Root.
- Actual mounted-device detection and real USB capacity/hotplug behavior. Native PRX modes and managed storage-query compatibility are corrected.
- Gold Vapor status and pulsing Preparing/Starting Play feedback. CS2/Blender runtime bundles remain unchanged.

## Acceptance and limits

The exact executable was installed/readback-verified; all 3,675 package hashes/modes were checked. Startup/runtime logs were reviewed, and the owner confirmed Explorer/device/storage/profile behavior. Personal copies were independently byte-compared with retained originals. Editor/file/profile/Office/XLSX host/native fixtures passed. Not every game or advanced workbook action was physically re-tested.

Enshrouded is owner-confirmed running after scoped recovery. CS2's earlier result is offline bots; official authentication/matchmaking remain unsupported. Blender's interactive repair is retained. Kane's Wrath is splash-only, Splitgate authentication-blocked, and co-op/LAN work unqualified. See [compatibility](compatibility.md) and the [multi-workstream report](reports/development-20261107.md).

## Downloads and updating

Download **WinBrew-11-Desktop-Storage-Dev-v1.zip**, **fix-vapor-install-modes.py**, **SHA256SUMS.txt** and **INSTALL.md** from this release. A first installation also needs the unchanged [Vapor-base-kit-0.13.zip](https://github.com/KPKev/WinBrew-11/releases/download/combined-blender-v1/Vapor-base-kit-0.13.zip); its hash is included in the new checksum list. Existing correctly installed base kits do not need replacement.

Unchanged runtime corresponding sources: [CS2](https://github.com/KPKev/WinBrew-11/releases/download/combined-blender-v1/cs2-runtime-corresponding-source.zip) and [Blender](https://github.com/KPKev/WinBrew-11/releases/download/combined-blender-v1/blender-runtime-corresponding-source.zip). The new source archive and Git commits contain the updated app/tests/docs. No commercial game files, account data, saves or private logs are included.

Close WinBrew and save work before copying the complete app. Back up the app and both private/new personal-file locations. First successful startup stages/verifies six known personal folders, retains originals and uses the published profile thereafter. Internal settings/Steam state stays private. Rollback to an older app does not copy new-profile edits back. No partition resizing occurs. Follow [installation](getting-started.md) and [migration](explorer-storage.md).
