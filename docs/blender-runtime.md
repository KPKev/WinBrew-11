# Blender compatibility bundle

The combined release includes a scoped Blender bundle for Steam app **365670**. The normal installer selects its full matched Wine 0.19 runtime, derived `win32u.prx`, Mesa provider and portable launcher. New installations and reinstalls use the same verified copy path. An older Ready installation must refresh its selected runtime before launch. Other games retain their existing runtime choices, including CS2's `cs2-019.1` repair.

The owner's installed test confirmed a stable 1080p workspace, smooth pointer and menus, and responsive mouse-wheel zoom. The owner confirms the earlier unexpected return to Home was resolved by the cursor repair. Large scenes, long sessions, rendering, saving and export are not certified by these input tests. The portable release launcher and installer were checked separately; the combined package has not replaced the owner's running app during release preparation.

## Included changes

- The launcher selects Blender by Steam identity and executable basename, independently of PS5 title number, install drive and folder.
- Blender defaults to Vulkan. An explicitly supplied `--gpu-backend` is retained, along with other arguments and script arguments. Mesa remains available for OpenGL startup probes.
- The launcher's loading animation cannot cover Blender's event-driven workspace.
- Wine updates the hardware cursor on mouse movement, cursor-shape changes and pointer repositioning instead of waiting for another graphics presentation. Non-Vulkan windows retain their existing fallback.
- Mouse-wheel detents are forwarded using the matched runtime's input ABI.
- Upstream sandbox discovery finds the current numbered app folder after relaunch.
- Normal WinBrew return requests target **PPSA77382**. Owner-only title IDs, paths, one-shot actions and per-frame timing probes are absent from the release launcher.

The bundle never supplies Blender itself, games, Steam credentials, projects or saves. Its installer replacement paths are restricted to runtime files and the pinned launcher/Mesa files; game descriptors, title parameters, profiles and bottle contents are outside that override. Existing project and settings preservation is covered by migration fixtures.

## Build and package

Follow the existing [source prerequisites](source-build.md). The runtime asset is `blender-runtime-019.1.zip`; its archive digest and complete file list are pinned by `tools/prepare_blender_runtime.py` and `assets/blender-runtime/manifest.json`. The source-build tool can download the pinned asset automatically from the [combined release](https://github.com/KPKev/WinBrew-11/releases/tag/combined-blender-v1), or accept a local copy:

```sh
python3 tools/prepare-blender-runtime.py --archive /path/to/blender-runtime-019.1.zip
bash tools/build-vapor.sh NEW_BUILD_NAME
```

The matching runtime and corresponding-source assets are published with the combined release. The source-build tool refuses a missing or mismatched asset instead of using the older Blender runtime. `tools/build-vapor-cs2.sh` remains a compatible full-build entry point and now includes both compatibility bundles.

`tools/create-blender-runtime-release.py` authors the payload from the pinned original 0.19 kit/Wine archives, compiled portable launcher and owner-confirmed cursor module. The resulting manifest and generated C++ table bind every copied file. The app assembler and ZIP packager independently require those pins. The base shared kit and mutable bottle template remain unchanged.

## Corresponding source

`runtime-patches/blender/source-receipt.json` records source and output hashes. `launcher-source.tar.gz` contains the complete modified launcher source, build script, assets and notices. The release's `blender-runtime-corresponding-source.zip` also contains the pinned Wine/prospero/foundation/FreeType sources, cursor patch and build scripts. No private console logs or recovered scene files are included.

The cursor module starts with the preserved, patched 7537eb0 Wine tree from the [CS2 corresponding-source procedure](../runtime-patches/cs2/README.md). Copy that prepared work directory to an isolated Blender work directory, apply `win32u-independent-cursor.patch` in its `wine-source`, then run `runtime-patches/blender/build-win32u.sh` and `link-win32u.sh` from the repository root. `BLENDER_WINE_WORK`, `PS5_PAYLOAD_SDK`, `BLENDER_PRX_FOUNDATION` and `BLENDER_NATIVE_TOOL` can select equivalent prepared build prerequisites. The module retains FreeType and Vulkan support. Shared CS2 source/build directories are not edited.

The launcher source archive uses a stable base revision and compiler source maps. Rebuilding its native target in a second directory produced the same executable SHA256. The fixture checks cover portable executable selection, explicit backends, argument preservation, input ABI, first install/reinstall, legacy refresh, project/settings preservation, missing/corrupt/linked sources and forbidden replacement destinations.

## Network scope

Official online play is unsupported and outside this work. Co-op and multiplayer testing are deferred. The current priority is making the applications and games operate locally; reaching a login or splash screen is not gameplay acceptance.

The sanitized [verification receipt](blender-verification.json) separates owner acceptance, installer fixtures, release hashes and qualification limits.
