# Get started with the current application

Current release line: **cs2-runtime-v1 / cs2-019.1**, November 7, 2026. The application is **PPSA77382**, WinBrew 11. Older PPSA77381 instructions are archived.

## Download and install

1. Use the [latest release](https://github.com/KPKev/WinBrew-11/releases/latest). Download `Vapor-CS2-019.1.zip`, `SHA256SUMS.txt`, and `fix-vapor-install-modes.py` if your extractor loses Unix permissions.
2. Verify the download against the published SHA-256 checksums. Keep the complete `PPSA77382` directory and install it at `/data/homebrew/PPSA77382` using the supported homebrew setup. Close WinBrew before updating. Preserve existing private account/settings/save data and retain a rollback copy of the old application.
3. **New Vapor setup:** also download `Vapor-base-kit-0.13.zip`. Its `playden-kit/manifest.json` belongs at `/data/wine11/kits/playden-kit/manifest.json`. Existing configured installations keep this dedicated base kit. Do not overwrite the separate Playden application's kit.
4. Preserve directory modes as 0755 and file modes as recorded in the pinned manifests. If extraction or FTP lost them, use the release's standard-library-only helper:

   ```text
   python fix-vapor-install-modes.py --host YOUR_PS5_IP --base-kit
   python fix-vapor-install-modes.py --host YOUR_PS5_IP --base-kit --apply
   ```

   The first command plans/checks only. The second restores and verifies pinned application/base-kit permissions. Omit `--base-kit` when it is already configured. See the [runtime guide](cs2-runtime.md) for scope and prerequisites.
5. Open WinBrew, then Vapor. Sign into your own Steam account to load your library. No games, account credentials, or saves are bundled.

The PS5 must already have its supported homebrew loader/payload environment. Downloading this repository does not set up or modify console system software.

## What to expect

- New games are detected by the live library service. Downloaded artwork/details are saved per game and reused.
- Use **Refresh game cache** on a game's detail page to request updated artwork, descriptions, and news. This is a presentation refresh, not a game reinstall.
- Steam app 730 automatically receives the complete bundled fixed runtime through normal preparation, including first installation/reinstallation. Other games retain their runtime selection and saved player settings.
- The owner reports **Schedule I working** and **CS2 offline bots working**. LAN/local multiplayer and Internet tunneling are untested. See [compatibility](compatibility.md).

## Source updates

For the latest source and documentation:

```text
git clone https://github.com/KPKev/WinBrew-11.git
cd WinBrew-11
git switch main
git pull --ff-only origin main
```

Review any local changes before pulling; do not discard them to follow this example. Prebuilt applications are release assets. `build/`, `dist/`, downloaded dependencies, and runtime caches are intentionally excluded from Git. Use the [source-build guide](source-build.md) to compile instead of using the release package.
