# File Explorer and console storage

The desktop-storage-dev-v1 development release uses Windows-style drive letters as aliases for real PS5 paths. It does not install Windows, repartition storage, create extra capacity or combine filesystems.

| Explorer location | Actual location / purpose |
| --- | --- |
| C: | `/data`, the large console storage previously displayed as D:. Free space is shared with games and other data on that filesystem. |
| Desktop, Documents, Downloads, Pictures, Music, Videos | `/data/WinBrew/Users/Default/<folder>`, below `C:\WinBrew\Users\Default`. |
| Console Root | `/`, for browsing other accessible locations. Permissions and read-only mounts still apply. |
| USB / extended storage | Actual mounted external roots, with their real filesystem capacity. Empty connection folders are hidden. |
| Internal app state | Remains in the anchored app-private area, rather than the old small C: card or personal Documents. |

C: and the former D: represent the same underlying large storage; the duplicate D: card is hidden. Installing or saving there reduces that filesystem's free space. Removing data from another USB/M.2 filesystem does not increase C:'s free space. System reservations and installation requirements still apply.

## Personal-folder migration

Before service threads start on the first successful launch, WinBrew scans the six known personal folders in its original private storage. It copies them into a fresh staging profile, including `.winbrew-trash` recovery contents, verifies copies and unchanged source metadata, then publishes the complete profile and version marker. Originals are retained; account, Steam state, settings and unrelated private files are not copied or deleted.

An existing verified profile remains authoritative on subsequent launches. New edits are not overwritten by an old original copy. Unknown destinations, unsupported links, changed sources, failed writes and stale staging directories are refused. Original files remain available and the app reports setup failure. Do not erase a stage or existing profile to force migration. Back up both locations and report the error.

The current implementation uses one **Default** WinBrew profile, not a multi-user Windows/PSN account system. Hiding the old private drive does not reclaim its allocation. Rolling back to an older app can show its original private Desktop instead of the new profile; new-profile edits are not automatically copied back.

## Browsing and file actions

Explorer has tabs, Back/Forward/Up, breadcrumbs, editable addresses, current-folder search, left-aligned navigation/file rows, Details/List/Tiles, sorting, selection and preview. Search filters the current folder; it is not recursive Windows indexing. Network lists mounted locations only; SMB/network discovery is not implemented.

Right-click files, folders, sidebar locations and drives for available actions. Context labels remain centered, including gray disabled entries. Use Copy/Cut/Paste, no-overwrite Rename, recoverable Delete, Properties, Open in new tab and New document/folder actions. See [file actions](file-context-actions.md) and [Notepad saving](notepad-save-as.md).

Removable roots are rechecked about every two seconds while Explorer is open. Entries appear after mounting and disappear after removal. Already observed devices are retained during brief credential-lock contention rather than blinking out during another filesystem operation. Disconnected listings and tree caches are invalidated.

C: and Console Root expose real console files. Use Vapor or the console uninstall flow for registered games; deleting their directories manually can leave broken registrations. File operations do not provide automatic recovery for every system edit.
