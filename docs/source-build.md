# Current source and build entry points

The default **main** branch is the current source tree. The `cs2-runtime-v1` release pins the complete runnable application/runtime artifacts. Later documentation commits do not change a published binary unless a new artifact is explicitly published.

**Only want to install it on your jailbroken PS5? Follow [the installation walkthrough](getting-started.md).** You do not need this page, Git, a compiler, or a PS5 SDK to use the ready-made release. This page is for developers rebuilding the app.

## Start with a fresh source checkout

Use Git to download the source:

```text
git clone https://github.com/KPKev/WinBrew-11.git
cd WinBrew-11
```

For PS5 builds, use an x86-64 Linux/WSL environment. The pinned LLVM fetcher extracts Ubuntu Noble amd64 packages, so this is not a native Windows PowerShell build. Have Git, Python 3, Bash, CMake, a C++20 host compiler, curl, unzip, and dpkg-deb available. Run build commands from the repository root.

Fetch the source/font/helper inputs first:

```bash
python3 tools/bootstrap.py
bash tools/fetch-steam-sdk.sh
python3 tools/fetch-steam-llvm.py
python3 tools/fetch-steam-compiler-rt.py
python3 tools/prepare-cs2-runtime.py
```

These commands download and verify their pinned inputs; they do **not** finish the full native build setup. Check the prerequisite list below before running the full target. Existing mismatched dependency checkouts must be reviewed rather than reset or silently replaced.

## Choose the right target

| Goal | Entry point | Requirements/scope |
| --- | --- | --- |
| Run the current PS5 application | Complete app ZIP/base kit from [Releases](https://github.com/KPKev/WinBrew-11/releases/latest) | Recommended for a fresh user setup. Follow [getting started](getting-started.md). |
| Build the Windows desktop preview | `tools/build-preview.cmd` / `Preview.cmd` | Visual Studio 2022 C++ tools and tracked/generated UI assets. This is not a PS5 game runtime. |
| Run focused desktop/cache/render checks | `bash tools/test-cache-performance.sh` | Linux/WSL, GCC C++20, Python, and pinned Playden source for backend tests. |
| Build the complete current Vapor PS5 app | `bash tools/build-vapor-cs2.sh NEW_BUILD_NAME` | The pinned native/Steam prerequisites below must already be prepared. It does not deploy to a console. |
| Reproduce the derived CS2 runtime module | `tools/prepare-cs2-rebuild.py` and release corresponding-source archive | See [runtime-patches/cs2/README.md](../runtime-patches/cs2/README.md). |

## Pinned prerequisites and fresh-clone scope

`python tools/bootstrap.py` fetches pinned open-source source/font/helper inputs listed in `dependencies.json` and checks their identities. It does **not** itself create every SDK, network archive, native base package, or dedicated host build needed for the full Steam-enabled application.

The full build consumes:

- Pinned PS5 payload SDK and cached LLVM 18 toolchain. See `tools/fetch-steam-sdk.sh`, `tools/fetch-steam-llvm.py`, and `tools/fetch-steam-compiler-rt.py` for their arguments and checks.
- Native application base/import descriptors, a correctly built title-specific root helper, and the original `dist/PPSA77381` assembly base. These are build inputs, not the current installed title ID.
- Console-probed Steam networking archives/headers/CA in `build/steam-network-tested-snapshot`, validated by `tools/verify-steam-tested-inputs.py` against `tools/steam-network-tested-inputs.json`.
- Generated dedicated runtime-host source/libraries and CMake setup used by `tools/prepare-steam-runtime-host-experiment.py`, `tools/prepare-steam-dedicated-host.py`, and `tools/refresh-steam-runtime-host.py`.
- The complete matched CS2 runtime. `python tools/prepare-cs2-runtime.py` acquires the public archive and verifies its fixed digest and all 1,760 entries. `--verify-only` checks an existing cache without downloading/rebuilding it.

A bare clone is **not** a one-command full native rebuild; the native/network prerequisites are still required. Do not substitute unverified archives or bypass failed hash checks. The complete prebuilt release is the immediate run path; the corresponding-source bundle supplies the runtime module's rebuild inputs while the advanced app-build setup is being streamlined.

In particular, this repository does not yet provide a complete fresh-machine recipe for producing the native assembly base, title-specific helpers, tested network snapshot, and configured dedicated-host CMake build. The commands above are a partial dependency bootstrap, not a claim that this remaining gap is solved. If those generated inputs are missing, use the prebuilt app for installation and report the missing build input rather than borrowing binaries from an unrelated title.

Check a prepared network snapshot with `python3 tools/verify-steam-tested-inputs.py`. The full target also expects `build/steam-host-dedicated-cmake/CMakeCache.txt`, the native boilerplate host tool, and the assembly/helper inputs consumed by `tools/assemble-steam-review.py` and `tools/assemble-steam-isolated-title.py`. The fetchers alone do not create these.

For the Windows preview, the current `tools/build-preview.cmd` calls Visual Studio 2022 Professional's `vcvars64.bat` at a fixed path. If you use another edition/location, adjust that local toolchain path first. The preview cannot verify PS5 installation, console payloads, or game compatibility.

Once prerequisites exist:

```bash
bash tools/build-vapor-cs2.sh my-current-vapor
python3 tools/package-vapor-cs2-release.py build/my-current-vapor-isolated/PPSA77382 my-current-vapor.zip
```

Use a new build name/output ZIP. These scripts verify runtime pins and package the complete application with recorded Unix modes. The historical `tools/build-ps5.sh` desktop-only output is not the current Steam/Vapor application. Do not distribute just `eboot.bin`.

## Git and Releases

Git contains application source, runtime source patches, tested selection logic, manifests, tools, regression tests, docs, and licenses. Generated outputs, dependency checkouts, runtime caches, console state, accounts, and game content are excluded. Releases supply complete app/base-kit archives, runtime payloads, corresponding source, and checksums. The [runtime guide](cs2-runtime.md) explains each asset.
