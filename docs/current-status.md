# Current verification — November 7, 2026

The **desktop-storage-dev-v1** development release contains the final owner-installed **PPSA77382** app (system version `01.016.000`). All **3,675 packaged files including the manifest** passed content/mode checks. The release tag distinguishes this update; the system version alone does not.

> [!WARNING]
> Previously working games can regress. **Skyrim Special Edition is currently owner-reported not working**, and another affected title remains unidentified. Earlier reports are not current-build guarantees. Check back often for fixes and review [compatibility](compatibility.md).

## Owner-confirmed desktop/storage

- Explorer opens; navigation/file layout was reviewed through owner screenshots.
- Real USB capacity, unplug removal and reconnect appearance confirmed; empty connection slots are hidden.
- C: represents the large console storage previously shown as D:. D: is hidden, Desktop files are present and Console Root is visible.
- First-profile migration copied and verified six known folders including recovery contents. Independent native comparison established byte equality with retained originals. Internal account/settings state was not relocated.
- Earlier Notepad opening/saving feedback is retained. Automated editor/workbook coverage is broader than the physically tested workflow.

## Game reports and limits

Enshrouded starts/runs after scoped recovery, owner-confirmed. CS2 offline bots, Schedule I, Blackjack 21, Golf With Your Friends and Quake II RTX have earlier working reports; this final build has not been re-tested against every game. Skyrim's earlier success is superseded by the current failure report. Blender's repaired workspace/input was previously confirmed. Kane's Wrath remains splash-only and Splitgate authentication-blocked. No new online/co-op compatibility is claimed.

## Automated/native verification

Host checks passed for actual Explorer/profile migration/restart, Notepad input/save/conflict/close, desktop contexts/preferences, file/tree failure retention, confirmed purge, Paint/CSV and XLSX interoperability. Native fixtures passed exact profile migration under UID1 with root-gated access, originals retained, recovery included and internal/game files excluded. The final native image added no undefined imports relative to the earlier owner-confirmed storage build.

Pinned CS2/Blender runtimes are retained. Native/runtime modes and package hashes were verified. Historical rendering figures are host benchmarks, not PS5 FPS. Every advanced application operation and game has not been physically re-tested.

## Installed identity

- Executable SHA256: `58ef324568b3235500d9baaa14bb7e78baefa30624427810a0feafd5e1ca673e`
- Manifest SHA256: `8f83086ba79a152c6926140828c45e361c8cc57e2894dac5a19c9e3c03d2b4a8`
- Matched CS2 ntdll remains `4f9f9fd45c65be0320794186f94bc2611be0d223199955a06d1e984cd295d94b`.

See the [development report](reports/development-20261107.md), [machine-readable summary](current-verification.json), [storage guide](explorer-storage.md), [Office limits](office-apps.md) and [release notes](release-notes-next.md). Private logs and owner documents are excluded.
