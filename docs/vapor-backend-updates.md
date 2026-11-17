# Vapor backend updates

Vapor checks for updates when WinBrew starts. **Backend updates**, beside **Library**, also lets you check whenever you want.

- **Backend update available:** a matched WinBrew/Vapor package is ready. Open the dialog and choose **Update backend**.
- **Playden update found:** upstream has a newer release, but a compatible Vapor package has not been published yet. The dialog explains this; a stock Playden package is not installed over WinBrew.
- **Restart to update:** the downloads have been checked and staged. Save your work, choose **Restart WinBrew**, then reopen it from PS5 Home.
- **Backend check failed:** the release check could not finish. Open the dialog and choose **Check now** to retry.

Updating and restarting wait for games, preparation, downloads, cloud operations and file operations to finish. A previous app/runtime pair is retained for **Restore previous version**. Game installations, bottles and saves are separate from the app update. No update can guarantee compatibility with every game.

The small gold text above **Preparing** shows the actual setup step and elapsed time. When the backend reports progress, its percentage and a bar appear. A percentage describes that step; it can start over for the next step. Unknown-duration steps use an animated bar without an invented percentage or time estimate.

After a runtime update, Play checks every game's owned title/runtime version. An older Ready game reaches preparation again so the host can refresh it automatically, preserving its bottle and scoped runtime selection.

## Package policy

The checker uses the public Playden latest-release endpoint for upstream discovery and KPKev/WinBrew-11's latest release for installable packages. The latter must include `vapor-backend-update.json`, a complete `PPSA77382/` app ZIP, and its matched runtime ZIP. The updater checks the release hashes, app identity and version, kit manifest and Vapor integration contract before staging. Manifest admission runs after HTTP redirects have finished.

The current migration derives Playden 0.11.0 (`7d30aaa7dc3011561eae728badc133a6bcc7f73b`) and kit 0.28. It retains WinBrew's namespace, metadata launch fixes, Blender shared folders and CS2/Blender scoped runtimes. The experimental upstream Steam host is not a claim that Dungeon Defenders II's online login works.

Public backend updates are qualified builds. Merely discovering a newer upstream source revision does not compile and qualify it on the PS5. A publisher must build the compatible package first; users then install that package from Vapor without rebuilding it themselves.

## Startup and library restoration

NativeSession confirms or recovers an update before constructing account, download or runtime workers. Update confirmation can temporarily raise the process UID; moving it after those workers start can stall startup behind a resumed download.

Saved download jobs wait for sign-in and the initial library load before taking the credential guard. They remain normal waiting jobs that can be paused or canceled. The controller keeps the latest library request identity after completion so Playden 0.11's later library updates can add newly resolved entries. Licensed desktop applications, including Blender, are admitted by metadata; HTTPS app-info lookup and name caching use the verified CA and private Vapor storage.

## Building and maintaining the integration

Keep the repository's build/git/console coordination locks. The legacy `vendor/playden-source` checkout supplies the preserved SDK and qualified networking inputs; the additional `playden-backend` dependency is pristine upstream source.

1. Run `python3 tools/bootstrap.py` for pinned dependencies and `python3 tools/fetch-steam-llvm.py` for LLVM 18.
2. Run `python3 tools/prepare-vapor-backend-source.py` into a fresh output directory. It verifies the exact upstream revision and reviewed patch receipt, applies the patch without fuzz, and copies current integration headers.
3. Configure the derived CMake project with the preserved SDK toolchain, qualified network prefix and the new dependency's `third_party/prospero-win` source. Build the native libraries. For host checks, use host-built networking/crypto, not console archives.
4. Run `tools/build-steam-native-ui-probe.sh`, then `tools/build-vapor-backend-candidate.py --source SOURCE --native-build BUILD --output CANDIDATE`. This compiles all Vapor adapters against that backend and links the complete frontend.
5. Derive the kit using `tools/prepare-vapor-backend-kit.py`. Every upstream file is checked. Only the reviewed helper, its manifests and the Mono namespace path may change. The production consumer must also accept the result.
6. Use `tools/assemble-vapor-backend-candidate.py` to combine the candidate ELF, verified full baseline and new runtime assets. It preserves and verifies the compatibility runtimes.
7. `tools/package-vapor-backend-update.py` verifies app/kit inventories and modes and creates the matched archives and update manifest. Without a qualification receipt binding the exact executable and kit hashes, its manifest says `candidate-not-console-tested` and Vapor refuses to install it.

Keep persistent build caches outside `/tmp`; a WSL restart clears that directory. Store transcripts and qualification results in `evidence/`. Publishing a release is a separate operation; packaging tools do not publish automatically.
