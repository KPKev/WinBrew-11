# Install WinBrew 11 and Vapor

Current release line: **notepad-start-recycle-dev-v1**, November 10, 2026. The application is **PPSA77382**, WinBrew 11. Older PPSA77381 instructions are archived.

Start with a **PS5 that already has its jailbreak running**. This guide explains how to install the ready-made app. You do not need to compile code or install Windows. WinBrew 11 is the desktop; Vapor is the Steam library inside it.

## Before you start

- **Start your usual jailbreak and homebrew payloads.** A payload is a small program loaded by your jailbreak. After a full PS5 restart, you may need to start them again before homebrew apps will run.
- **Load [kstuff](https://github.com/EchoStretch/kstuff-lite)** for game execution. Start it again after a full console restart if your jailbreak loader does not do so automatically.
- **Keep [elfldr](https://github.com/ps5-payload-dev/elfldr) running on port 9021.** It runs the privileged helper used to give the app/runtime access to files. Many jailbreak loaders start it automatically; a successful Steam login does not prove this helper is available.
- **Have a homebrew loader that adds apps from `/data/homebrew` to Home.** Adding the tile is called registration. This project's installation history uses ShadowMountPlus. If you need it, follow the [ShadowMountPlus instructions](https://github.com/drakmor/ShadowMountPlus) for your firmware and its required payloads. Its compatibility list does not prove WinBrew works on every listed firmware.
- **Check the PS5's DNS settings.** Open **Settings → Network → Settings → Set Up Internet Connection**, open the options for your connected network, and look under **Advanced Settings → DNS Settings**. If DNS points to **`127.0.0.1`**, load **[nanoDNS](https://github.com/drakmor/nanoDNS)** or another local DNS server before opening Vapor. That address points to the PS5 itself; without a DNS server listening there, Steam addresses cannot resolve. If another working DNS server is already configured, nanoDNS is not inherently required. Follow your jailbreak setup's networking instructions rather than changing DNS blindly.
- **Use a computer on the same local network as the PS5.** You need a browser and a ZIP extractor. Avoid isolated guest Wi-Fi, which can stop devices from reaching each other.
- **Run [PS5 Web File Manager](https://github.com/owendswang/ps5-web-file-manager) on the PS5.** Follow that project's instructions to load its payload. This walkthrough uses its browser upload feature. FTP can copy files instead, but the permission helper below still needs this Web File Manager running.
- **Install Python 3 on your computer** for the permission helper. Download it from [python.org](https://www.python.org/downloads/). On Windows, enable the installer's PATH option if offered, then open a new terminal and type `python --version`. Try `py` on Windows or `python3` on Linux/macOS if `python` is not found. Use that working command in place of `python` below. No extra Python packages are needed.
- **Check free space on the PS5's internal storage.** Allow room for both extracted archives, game downloads/installation, and an old-app backup if updating. The extracted folders take more space than the ZIP downloads; space needed for games varies.
- **Have your own Steam account and Internet access on the PS5** for sign-in, library access, and downloads. Own any paid game you intend to download. Games, accounts, and saves are not included.

You do not need to install the separate Playden app for this release. The complete app includes WinBrew's helper; the base-kit download supplies its shared game runtime. Keep any existing Playden installation separate.

These console prerequisites also appear in the [upstream Playden requirements](https://github.com/leolobato/playden-ps5#requirements). Use payload versions appropriate for your firmware; upstream testing is not a WinBrew firmware compatibility guarantee. After a full restart, reload the jailbreak, kstuff, ShadowMountPlus, elfldr, and nanoDNS if your DNS setup uses it. Then launch WinBrew/Vapor. Copying the app or seeing an existing Home tile does not start these payloads.

## Connect to your PS5 first

1. On the PS5, open **Settings → Network → Connection Status → View Connection Status**. Write down its IPv4 address, for example `192.168.1.50`.
2. Start PS5 Web File Manager through your payload loader. Read the port number in its startup notification. A port is the number after the colon in a web address.
3. On your computer, open `http://YOUR_PS5_IP:PORT` in a browser. Replace both placeholders with your console's actual address and port. For example: `http://192.168.1.50:8889`.
4. You should see a file list and be able to open `/data`. If it does not open, use [connection troubleshooting](troubleshooting.md#the-computer-cannot-open-the-ps5-file-manager) before continuing.

Web File Manager starts looking for a free port at **8888** and can choose **8889** or another port. Use the port it actually reports. An FTP server's port is not this browser/API service.

## 1. Download the right files

On your computer, open the [notepad-start-recycle-dev-v1 development release](https://github.com/KPKev/WinBrew-11/releases/tag/notepad-start-recycle-dev-v1). Save the new app, checksum list and matching permission helper from **Assets**. For a first installation, also download the unchanged [base kit](https://github.com/KPKev/WinBrew-11/releases/download/combined-blender-v1/Vapor-base-kit-0.13.zip). Keep these four files together:

| File | What it is for |
| --- | --- |
| `WinBrew-11-Notepad-Start-Recycle-Dev-v1.zip` | The complete app, including the fixed CS2 and Blender runtimes. |
| `Vapor-base-kit-0.13.zip` | The shared game runtime needed for a first installation. |
| `SHA256SUMS.txt` | Expected download fingerprints, used to check the files. |
| `fix-vapor-install-modes.py` | Checks/restores file permissions after copying. |

Leave the other assets alone for a normal installation. The separate Blender runtime ZIP and compatibility modules are already included in the complete app. **Source code (zip)**, **Source code (tar.gz)**, and **Code → Download ZIP** are developer files, not the ready-made console app.

### Check the downloads

A SHA-256 checksum is a file's fingerprint. Checking it catches incomplete or changed downloads.

On Windows, open the download folder, right-click empty space, and choose **Open in Terminal**. In PowerShell run:

```powershell
Get-FileHash .\WinBrew-11-Notepad-Start-Recycle-Dev-v1.zip -Algorithm SHA256
Get-FileHash .\Vapor-base-kit-0.13.zip -Algorithm SHA256
Get-FileHash .\fix-vapor-install-modes.py -Algorithm SHA256
```

Open `SHA256SUMS.txt` in a text editor. Compare each entire **Hash** result with the entry for that filename. Uppercase/lowercase does not matter; everything else must match. If a file has no entry, check the release's published digest for that asset instead. If a checksum differs, download that file again before using it.

On Linux, use `sha256sum FILENAME`; on macOS, use `shasum -a 256 FILENAME`. Replace `FILENAME` with each actual filename above and compare with the same list.

## 2. Extract both ZIPs on your computer

1. Right-click `WinBrew-11-Notepad-Start-Recycle-Dev-v1.zip` and choose **Extract** or **Extract All**. Find the **PPSA77382** folder inside the extracted files.
2. Extract `Vapor-base-kit-0.13.zip` separately. Find the **playden-kit** folder.
3. Check that `PPSA77382` contains `eboot.bin`, `manifest.json`, `sce_sys`, and `runtime`. Check that `playden-kit` contains `manifest.json` and its runtime files.

Keep every file and subfolder, with their original names. Copying only `eboot.bin` is incomplete. Extracting on the computer avoids needing Web File Manager's separate archive-extraction helper.

## 3. Copy the folders to the PS5

In Web File Manager on your computer:

1. Open `/data`, then `homebrew`. If `homebrew` does not exist, create it using the new-folder option.
2. Upload the extracted **PPSA77382** folder into `homebrew`, using folder upload or by dragging the folder into the browser. Wait for the transfer job to finish successfully.
3. Open `/data`. Create/open `wine11`, then create/open `kits` inside it.
4. Upload the extracted **playden-kit** folder into `kits`. Wait for this transfer to finish too.
5. Browse the copied folders and check these exact paths:

```text
/data/homebrew/PPSA77382/eboot.bin
/data/homebrew/PPSA77382/manifest.json
/data/homebrew/PPSA77382/sce_sys/param.json
/data/homebrew/PPSA77382/runtime/compatibility/steam-730-cs2-v1/vapor-runtime.json
/data/homebrew/PPSA77382/runtime/compatibility/steam-365670-blender-v1/vapor-runtime.json
/data/wine11/kits/playden-kit/manifest.json
```

The result should look like this, with all the archive's other files kept too:

```text
/data/
  homebrew/
    PPSA77382/
      eboot.bin
      manifest.json
      sce_sys/
      runtime/
  wine11/
    kits/
      playden-kit/
        manifest.json
```

Watch for extra folders: `/data/homebrew/PPSA77382/PPSA77382/eboot.bin` is wrong because `PPSA77382` appears twice. The ZIP filename should not become an extra folder on the PS5 either. Check the paths even when the upload says it succeeded.

The base kit belongs under **`/data/wine11`**, not `/data/playden`. Do not overwrite a separate Playden app's kit. The old `tools/install_console.py` script targets PPSA77381; it is not the installer for this release.

## 4. Check and restore file permissions

Permissions tell the PS5 which files it can read or run. Some ZIP extractors and FTP transfers lose these settings. The supplied helper restores the settings recorded for this release. Its manifest files are included checklists that identify the release and its files.

1. Keep Web File Manager running and **keep WinBrew closed**.
2. On your computer, open a terminal in the folder containing `fix-vapor-install-modes.py`.
3. Run this command. Replace `YOUR_PS5_IP` and `PORT` with the address and port that opened the file manager:

```text
python fix-vapor-install-modes.py --host YOUR_PS5_IP --port PORT --base-kit
```

For the earlier example, that becomes:

```text
python fix-vapor-install-modes.py --host 192.168.1.50 --port 8889 --base-kit
```

This first command only checks. It should print `Verified release manifests; ... packaged paths need mode changes.` If it reports **0** paths need changes, no repair is needed. If changes are needed, run the same command with `--apply` added:

```text
python fix-vapor-install-modes.py --host YOUR_PS5_IP --port PORT --base-kit --apply
```

Success ends with `PASS: packaged file modes verified; no file contents, games, accounts or saves changed.` See [troubleshooting](troubleshooting.md) if it reports an error.

The helper checks that the release checklists match, the listed paths exist, and the permissions are correct. It **does not upload missing files or check every copied file's contents**. A passing result does not replace checking the download fingerprint and transfer results. Do not edit its checks to force it to accept a different release.

## 5. Get the tile on Home and launch

1. Make sure your homebrew loader is running and scans `/data/homebrew`. ShadowMountPlus includes this folder in its [default scan locations](https://github.com/drakmor/ShadowMountPlus#scan-paths). Allow its scan to finish, or use its scan/rescan control if your version provides one; the name depends on the loader/version.
2. Return to PS5 Home and look for **WinBrew 11**. This is a folder-based app, so there is no `.pkg` to install with the system package installer.
3. Open it. Confirm that the desktop and taskbar appear and that you can move the pointer or navigate with your controller. USB mouse and keyboard input are also supported.
4. Open **Vapor** from inside WinBrew's desktop or Start menu. Follow its Steam sign-in prompts, including Steam Guard if requested.
5. Wait for your owned library to load. Artwork may arrive afterward.

If the tile is missing, use [troubleshooting](troubleshooting.md#there-is-no-winbrew-11-tile-on-home). Copying the folder and registering its tile are separate steps; uploading again does not fix a stopped loader.

## 6. First-launch checks and your first game

Before starting a large download, check:

- [ ] WinBrew opens to a usable desktop and input responds.
- [ ] Vapor opens and Steam sign-in completes.
- [ ] Your library loads and a game's detail page opens.
- [ ] The PS5 has enough internal storage for the download and installation.

Choose a game in your library and open its details. Use its offered install/download action. Let downloading and preparation finish before using the launch/play action. Check the [compatibility list](compatibility.md) when choosing a first test: owning/downloading a Windows game does not guarantee it runs through Wine.

After testing a game, close it normally. Reopen WinBrew/Vapor if needed and confirm your library is still there. Successfully cached artwork/details should be reused. If Vapor works but a specific game fails, follow [game-launch troubleshooting](troubleshooting.md#vapor-works-but-a-game-will-not-launch).

Vapor's Steam sign-in is for library/download services. It does not provide an authenticated Windows Steam client to every game. CS2's official in-game Steam authentication/matchmaking remains unsupported even when Vapor sign-in succeeds.

## Updating an existing installation

1. Finish or pause active work, close games and WinBrew normally, and return to PS5 Home before changing files.
2. Back up `/data/homebrew/PPSA77382` to your computer. Separately back up private account/settings data, game bottles, and saves using your normal backup process. A bottle is the per-game Wine environment containing Windows-style settings and related data; the app-folder backup alone does not cover all of it.
3. Download and check the complete replacement app archive. Copy its complete `PPSA77382` contents into the matching app folder, replacing release files only. Keep the old backup until the update works.
4. Keep the existing `/data/wine11/kits/playden-kit` unless the new release explicitly requires a base-kit change. Do not delete `/data/wine11`, game folders, accounts, or saves as an update step.
5. Repeat the permission check/repair. Omit `--base-kit` if the existing kit is already correctly configured.
6. Repeat the desktop, input, Vapor sign-in/library, and game-launch checks. An app update does not require redownloading every game.

This release also copies/verifies your six known personal folders into `/data/WinBrew/Users/Default` on first successful startup. Originals are retained, and internal account/settings data is not relocated. Back up this new personal-files folder separately; restoring an older app may show its older private Desktop instead. New-profile edits are not automatically copied back. See [storage and migration](explorer-storage.md).

To roll back, close WinBrew and restore the backed-up app folder and its recorded permissions, preserving game/account/save data. Permission helpers are release-specific: use the matching helper/instructions for the version being restored.

## Desktop storage and file tools

Open **This PC** for the large console-backed C:. The separate D: card is hidden. Desktop/Documents/Downloads/Pictures/Music/Videos use `C:\WinBrew\Users\Default`; the Personalization display name defaults to WinBrew and changing it does not move that folder; **Console Root** browses other accessible paths. USB entries appear only for actual mounted devices, with real capacities, and disappear after unplugging. This is a real-filesystem desktop environment, not Microsoft Windows or a repartitioning utility. See [Explorer/storage](explorer-storage.md) and [Office capabilities](office-apps.md).

> [!WARNING]
> **Active development: game compatibility can change between builds.** Some previously working games may temporarily fail to launch or behave unexpectedly after code changes. **Skyrim Special Edition is currently reported not working.** Earlier compatibility reports are historical results, not guarantees for this build. Please check back often for updates and fixes.

## What to expect

- New games are detected by the live library service. Downloaded artwork/details are saved per game and reused.
- Use **Refresh game cache** on a game's detail page to request updated artwork, descriptions, and news. This is a presentation refresh, not a game reinstall.
- Steam app 730 automatically receives the complete bundled fixed runtime through normal preparation, including first installation/reinstallation. Blender (Steam app 365670) also receives its pinned runtime and launcher through normal preparation. Other games retain their runtime selection and saved player settings.
- Earlier owner reports include Schedule I, CS2 offline bots, Blackjack 21 and Golf With Your Friends. **Skyrim Special Edition is now reported not working**, despite its earlier successful result. Other regressions may affect previously compatible games. Online play is unavailable for the listed games; official online support is not pursued and co-op is deferred. See [compatibility](compatibility.md).

## Source updates

The steps above install the ready-made app. The commands below are only for people who want the source code. For install problems, use [troubleshooting](troubleshooting.md).

For the latest source and documentation:

```text
git clone https://github.com/KPKev/WinBrew-11.git
cd WinBrew-11
git switch main
git pull --ff-only origin main
```

Review any local changes before pulling; do not discard them to follow this example. Prebuilt applications are release assets. `build/`, `dist/`, downloaded dependencies, and runtime caches are intentionally excluded from Git. Use the [source-build guide](source-build.md) to compile instead of using the release package.
