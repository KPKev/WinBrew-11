# Changelog

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
