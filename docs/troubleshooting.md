# Installation and first-launch troubleshooting

## CS2 says runtime bundle is unavailable

Update the complete app to [combined-blender-v1](https://github.com/KPKev/WinBrew-11/releases/tag/combined-blender-v1), then press Play in Vapor and let preparation finish. The original automatic-runtime release could skip preparation for an older CS2 installation still marked Ready. The hotfix checks its selected runtime before allowing that shortcut.

If preparation says a title file could not be replaced after a manual Wine-folder replacement, check runtime-folder ownership. The file manager can create folders owned by UID 0 while Vapor's title belongs to UID 1. On the owner's installation, correcting ownership of 29 runtime folders allowed the managed refresh to finish, preserving the launcher, fixed module, profiles and saves. The app/base-kit permission helper does not change ownership of installed game titles. This legacy case needs a scoped repair bound to the title's owner marker, rather than a game reinstall or blanket permissions change. Normal Vapor installs create runtime folders under the app's user.

See the [verified repair and installed-title audit](reports/cs2-handoff-20261107.md). Other titles retain their existing runtime selection and profiles.

These checks match **combined-blender-v1 / PPSA77382**. Start with the [installation guide](getting-started.md) if you have not copied the complete app and dedicated base kit yet.

## The computer cannot open the PS5 file manager

1. Check the PS5's current IPv4 address in its network settings. It may have changed since your last session.
2. Confirm that PS5 Web File Manager is running. Open `http://YOUR_PS5_IP:PORT`, using the port in its startup notification. It starts at 8888 but can choose 8889 or another free port.
3. Put both devices on the same local network. Guest Wi-Fi/device isolation can prevent them from reaching each other. If your computer uses a VPN, check whether it blocks local-network access.
4. Confirm that you opened the HTTP file manager, not an FTP server's port. The permission helper uses [owendswang's Web File Manager API](https://github.com/owendswang/ps5-web-file-manager).

If a transfer times out, inspect its job in the file manager before starting another copy. A timeout does not tell you whether the original job stopped or finished.

## There is no WinBrew 11 tile on Home

- Check that `/data/homebrew/PPSA77382/sce_sys/param.json` exists. An extra `PPSA77382` or ZIP-named folder means the upload is nested incorrectly.
- Confirm that the full transfer finished and the permission check passes.
- Check that your loader is running and scanning `/data/homebrew`. For ShadowMountPlus, see its [scan-path instructions](https://github.com/drakmor/ShadowMountPlus#scan-paths) and allow its scan to finish.
- After a full PS5 restart, start your usual jailbreak and required payloads again. An existing Home tile does not prove the current jailbreak session is ready.

Do not edit `app.db`, rename title IDs, or install the old PPSA77381 package to fix a missing tile. The current release is PPSA77382.

## The permission command will not run

| What you see | What to do |
| --- | --- |
| `python` not found / Python opens the Store | Install Python from [python.org](https://www.python.org/downloads/), reopen the terminal, and check `python --version`. Try Windows `py` or Linux/macOS `python3` if appropriate. |
| Cannot open `fix-vapor-install-modes.py` | Open the terminal in the folder where you saved it, or give its full path. Check the filename ends in `.py`, not `.py.txt`. |
| Argument error for `PORT` | Replace `PORT` with the actual number, for example `--port 8889`. Replace `YOUR_PS5_IP` too. Do not type the placeholder words literally. |
| Connection refused, timeout, or unexpected response | Open the same address/port in a browser and confirm it is the correct Web File Manager. Keep it running while the helper works. |

## The permission helper rejects the installed files

| Message or symptom | Meaning and next step |
| --- | --- |
| `Installed manifest differs from this release` | The helper is pinned to a specific release. Check that the app, base kit, and helper match. Verify download checksums and recopy the correct files if needed. Do not change the helper's pins. |
| `Missing or linked path` | A required path is absent or is a link. Check the printed path, folder nesting, and transfer jobs. Recopy the matching release's regular files/directories from the verified archive. The helper cannot upload them. |
| Paths need mode changes | Close WinBrew, then run the same command with `--apply`. |
| `Completion unknown` or some modes remain incorrect | Inspect the file manager's task status and connection first. Once the previous task's outcome is known, rerun the read-only check before any further repair. |

Directories normally use `0755`; files use the individual `0644` or `0755` settings recorded for the release. Use the helper instead of setting everything to `0777`. It checks manifest identities, path types, and modes; it does not hash every uploaded payload file. If corruption is suspected, verify the original download and compare/retransfer affected files while the app is closed.

Native libraries in `sce_module/*.prx`, including `libc.prx`, require `0755`.
Setting `sce_module/libc.prx` to `0644` causes the PS5 loader's
`mount flag / attribute error` before the desktop starts, even when its file
hash is correct. The permission helper and app ZIP packager preserve that
executable permission. A launch API success or a live process alone does not
establish that the desktop has loaded; check the loader log and visible UI.

## The tile appears, but WinBrew closes or shows a black screen

1. Record the exact error and when it happens. Check that the jailbreak and your loader's required payloads are running for this session.
2. Check that you opened the current **PPSA77382** tile and copied the complete app. It needs its helper, assets, libraries, and runtime as well as `eboot.bin`.
3. Close the app before repairing its files. Check download fingerprints, folder layout, and permissions. Back up an existing install before replacing files.
4. If it still fails, report the issue using the checklist below. Do not repeatedly launch a crashing app or swap in helpers/libraries from unrelated titles.

There is no published WinBrew firmware compatibility matrix proving this app on every jailbroken PS5. Include firmware and payload versions in a report so a console-environment issue can be distinguished from an incomplete copy.

## The desktop opens, but Vapor will not sign in or load the library

- Confirm the PS5 itself has Internet access; the computer's connection alone is not enough.
- If the PS5's DNS is `127.0.0.1`, confirm [nanoDNS](https://github.com/drakmor/nanoDNS) or another DNS server is running on that address for this boot. Without it, Steam hostnames cannot resolve. A different working DNS setup does not require nanoDNS. See [payload and DNS prerequisites](getting-started.md#before-you-start).
- Complete the Steam sign-in prompts, including Steam Guard when requested. Record the exact error if it fails.
- If the error mentions certificates or TLS, check the PS5's date/time and that `assets/steam/cacert.pem` is present from the original app package. Do not disable certificate verification or substitute an unrelated file.
- Check the complete app was copied. Desktop startup alone does not prove the Steam services initialized successfully.
- If games are listed but artwork is missing, allow it to load or use **Refresh game cache** on the affected game's detail page. This does not repair authentication, change ownership, or reinstall the game.

Keep saved account/sign-in data while diagnosing this. Hide passwords, Steam Guard codes, QR sign-in images, session tokens, and private account files before sharing a report.

## Vapor signs in, but says "Install and launch not connected yet"

This message does not identify one cause. Steam account/library access and the download/game services initialize separately; Vapor can retain account access when its game-storage folder cannot be opened. A saved login or cached library also does not prove the PS5 is currently online.

1. Confirm kstuff, ShadowMountPlus, and elfldr (port 9021) are running for this boot. If DNS is `127.0.0.1`, confirm nanoDNS is running too. See [prerequisites](getting-started.md#before-you-start).
2. Confirm Vapor has completed its current Steam sign-in and the PS5 can reach the Internet.
3. Confirm you installed the complete matching app and base kit at `/data/homebrew/PPSA77382` and `/data/wine11/kits/playden-kit`, without an extra nested folder. Close WinBrew before repairing files and run the matching release's [permission check](getting-started.md#4-check-and-restore-file-permissions) with `--base-kit`.
4. Reopen WinBrew after any repair and check the game page again. **Refresh game cache** updates game details/artwork; it does not reconnect the download service.
5. If it persists, report your release/ZIP name, firmware, payload versions, exact message/screenshot, and permission-check output. Keep account files, games, bottles, and saves intact.

The packaged permission checker covers app/base-kit files. It does not validate or repair every parent-folder ownership setting or private game-storage permission. A passing result can therefore still require a startup-log and storage-access investigation; do not recursively change `/data/wine11` permissions or delete it to try to reconnect downloads.

## Vapor works, but a game will not launch

1. Check [compatibility](compatibility.md). A game being in your Steam library does not establish that its Windows executable works through Wine.
2. Confirm download and preparation finished, and check internal storage. Record any preparation/launch error.
3. Check `/data/wine11/kits/playden-kit/manifest.json`. Another app's kit under `/data/playden` does not satisfy this path. Run this release's permission check with `--base-kit`.
4. For **CS2 / app 730**, check the complete app's `runtime/compatibility/steam-730-cs2-v1` folder. Normal install/reinstall selects the fixed runtime automatically. Missing, changed, or linked runtime files stop preparation; restore the matching app package first.
5. Back up game settings, bottles, and saves before using a reinstall/recovery flow. Do not delete them as an app-update troubleshooting step. One game's failure does not automatically require reinstalling WinBrew.

CS2 offline bots are owner-confirmed. Official in-game Steam authentication and matchmaking are unsupported, even if Vapor's own sign-in succeeds. LAN/local multiplayer and Internet tunneling are untested; they are not required setup steps.

## Reporting a problem

Open [WinBrew-11 Issues](https://github.com/KPKev/WinBrew-11/issues) and include:

- App release/ZIP filename, PS5 firmware, and loader/payload names and versions.
- First install or update, and how you copied the files.
- Whether checksums matched, the destination paths, and permission-helper output.
- Last working step: Home tile, desktop, input, Vapor sign-in, library, download/preparation, or game launch.
- Exact error text, a screenshot with private information hidden, and what you tried. For a game issue, include its name and Steam app ID.

## Blender repair

The combined release supplies the pinned Blender launcher and runtime during normal installation/reinstallation. It includes Vulkan defaults, independent cursor updates, wheel forwarding and relaunch-folder discovery. Confirmed scope is interactive workspace/pointer/menus/zoom, not large-scene or render/export stability. The owner confirms the earlier unexpected exit was resolved by the cursor repair. See [Blender runtime details](blender-runtime.md).

Counter-Strike's working result is offline play. Splitgate currently stops at server authentication; Kane's Wrath reaches only its first splash screen. Official online work is outside the scope, and co-op testing is deferred.

## A previously compatible game stopped working

This is an active development build. Skyrim Special Edition is currently owner-reported not working after recent changes; the cause is not confirmed. Another affected title has not yet been identified. Earlier successful results remain historical reports, not guarantees for every release. Check [compatibility](compatibility.md) and newer releases often. Report the exact game, last working release and current failure; keep bottles, settings and saves intact.

Enshrouded's `owned validated template`, changing-settings and frozen-operation messages required investigation of an installed preparation transaction. The owner eventually confirmed the game running after scoped recovery. This is not permission to delete arbitrary journals, force a game Ready, or erase a Wine environment; report the state instead of guessing a repair.

## Explorer crashes or shows phantom 2 MB drives

Use the current development package and its matching helper. Earlier local candidates used an unsupported managed-title storage query; this release uses the checked libkernel query and preserves native PRX executable modes. Empty `/mnt/usbN` folders belong to a containing filesystem and are not real connected drives. Current detection requires the actual mount identity; USB devices should appear/disappear within about two seconds while Explorer is open.

## Where did D: and my old small C: go?

C: now represents `/data`, the same large console storage previously shown as D:. No partition was resized. Personal folders use `/data/WinBrew/Users/Default`, and Console Root provides broader accessible paths. The original small private area still holds internal settings/account state and retained original personal files; hiding its old drive card does not free that filesystem allocation.

If personal storage setup fails, WinBrew reports the error and leaves the original files available. Do not delete an unknown existing profile or a `.migrating-v1` stage to force progress. Back up both locations and report the error. See [migration details](explorer-storage.md).
