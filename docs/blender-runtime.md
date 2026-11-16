# Blender compatibility bundle

The combined release includes a scoped Blender bundle for Steam app **365670**. The normal installer selects its full matched Wine 0.19 runtime, derived `win32u.prx`, Mesa provider and portable launcher. New installations and reinstalls use the same verified copy path. An older Ready installation must refresh its selected runtime before launch. Other games retain their existing runtime choices, including CS2's `cs2-019.1` repair.

The owner confirmed the repaired workspace, pointer/menus, inline Save As, direct Documents/Desktop save/reopen and shared-file recycle deletion on the preceding installed build. The general automatic 019.3 candidate has passed installer/upgrade/enrollment tests and actual PS5 migration/rollback fixtures. Final normal-launch and corrective recycle/old-project acceptance are recorded in the release verification receipt. Large scenes, long sessions, rendering and exporting remain outside that qualification.

Documents and Desktop now use the real WinBrew profile. Wine's `prospero` shortcuts are aliases of those folders; old private folders are preserved as `*.before-WinBrew`. New installations and supported upgrades perform setup automatically. Open Vapor and press Play once after updating before launching from PS5 Home.

## Included changes

- The launcher selects Blender by Steam identity and executable basename, independently of PS5 title number, install drive and folder.
- Blender defaults to Vulkan. An explicitly supplied `--gpu-backend` is retained, along with other arguments and script arguments. Mesa remains available for OpenGL startup probes.
- The launcher's loading animation cannot cover Blender's event-driven workspace.
- Wine updates the hardware cursor on mouse movement, cursor-shape changes and pointer repositioning instead of waiting for another graphics presentation. Non-Vulkan windows retain their existing fallback.
- Mouse-wheel detents are forwarded using the matched runtime's input ABI.
- Save As and Preferences use temporary editors in the main Blender window, avoiding the black secondary window.
- Upstream sandbox discovery finds the current numbered app folder after relaunch.
- Normal WinBrew return requests target **PPSA77382**. Owner-only title IDs, paths, one-shot actions and per-frame timing probes are absent from the release launcher.

The bundle never supplies Blender itself, games, Steam credentials, projects or saves. Its installer replacement paths are restricted to runtime files and the pinned launcher/Mesa files; game descriptors, title parameters, profiles and bottle contents are outside that override. Existing project and settings preservation is covered by migration fixtures.

## Build and package

Follow the existing [source prerequisites](source-build.md). The runtime asset is `blender-runtime-019.3.zip`; its archive digest and complete file list are pinned by `tools/prepare_blender_runtime.py` and `assets/blender-runtime/manifest.json`. The 019.3 runtime is supplied with this release; provide the pinned archive for a source build:

```sh
python3 tools/prepare-blender-runtime.py --archive /path/to/blender-runtime-019.3.zip
bash tools/build-vapor.sh NEW_BUILD_NAME
```

The 019.3 runtime and matching corresponding-source assets are supplied together; the older combined release contains 019.1. The source-build tool refuses a missing or mismatched asset instead of using the older Blender runtime. `tools/build-vapor-cs2.sh` remains a compatible full-build entry point and now includes both compatibility bundles.

`tools/create_blender_shared_release.py` assembles 019.3 from the pinned 019.2 payload and rebuilt portable launcher/helper. All unchanged Wine/Mesa bytes stay pinned. The historical `tools/create-blender-runtime-release.py` produces the older base and is not the final release entry point. The resulting manifest, generated C++ tables and helper policy bind every copied file. App assembly and ZIP packaging independently enforce these pins. The base shared kit and mutable bottle template remain unchanged.

The helper uses a dedicated root-owned Blender allow-list. WinBrew enrolls only a prepared Steam 365670 record bound to the public runtime receipt. The helper validates the canonical profile marker and directory ancestry, normalizes visible Documents/Desktop entries to shared group 365670, preserves owner/content, and rolls back metadata changes after an ordinary failure. It refuses unsupported types/ownership and hardlinked regular files. It does not follow symbolic links. Unrelated hidden entries are preserved; `.winbrew-trash` and its contents are explicitly shared so both applications can access recycled files and recovery metadata. A missing profile is deferred for WinBrew's existing first-run initializer and refused for Blender. The launcher connects the Wine folder aliases only after verified group access.

## Corresponding source

`runtime-patches/blender/automatic-bridge-source-receipt.json` and `source-receipt.json` record source and output hashes. `launcher-source.tar.gz` contains the complete modified launcher source, build script, assets and notices. The release's `blender-runtime-corresponding-source.zip` also contains the pinned Wine/prospero/foundation/FreeType sources, cursor patch and build scripts. No private console logs or recovered scene files are included.

The cursor module starts with the preserved, patched 7537eb0 Wine tree from the [CS2 corresponding-source procedure](../runtime-patches/cs2/README.md). Copy that prepared work directory to an isolated Blender work directory, apply `win32u-independent-cursor.patch` in its `wine-source`, then run `runtime-patches/blender/build-win32u.sh` and `link-win32u.sh` from the repository root. `BLENDER_WINE_WORK`, `PS5_PAYLOAD_SDK`, `BLENDER_PRX_FOUNDATION` and `BLENDER_NATIVE_TOOL` can select equivalent prepared build prerequisites. The module retains FreeType and Vulkan support. Shared CS2 source/build directories are not edited.

The launcher source archive uses a stable base revision and compiler source maps. The earlier 019.2 target was byte-identical across two build directories; that historical result is not a claim of a second-directory 019.3 rebuild. The fixture checks cover portable executable selection, explicit backends, argument preservation, input ABI, first install/reinstall, legacy refresh, project/settings preservation, missing/corrupt/linked sources and forbidden replacement destinations.

## Network scope

Official online play is unsupported and outside this work. Co-op and multiplayer testing are deferred. The current priority is making the applications and games operate locally; reaching a login or splash screen is not gameplay acceptance.

The sanitized [verification receipt](blender-verification.json) separates owner acceptance, installer fixtures, release hashes and qualification limits.

## Rebuilding the 019.3 launcher and helpers

Use the qualified LLVM/PS5 SDK and prepared foundation from the source prerequisites. Extract `launcher-source.tar.gz` into an isolated source directory. Build both helpers from this release's helper source:

```sh
python3 runtime-patches/blender/winbrew-bridge-helper/tools/build_owned_daemon.py --sdk "$PS5_PAYLOAD_SDK" --logging-client vendor/ps5log --elf-helper --title PPSA77382
python3 runtime-patches/blender/winbrew-bridge-helper/tools/build_owned_daemon.py --sdk "$PS5_PAYLOAD_SDK" --logging-client vendor/ps5log --elf-helper --title-rule FAKE78 --allow-list /data/wine11/title-rule/blender-shared/allow-list
```

For the extracted launcher's `tools/build_native.sh`, set `PS5_CLANG` and `LLVM_CONFIG` to the pinned compiler, `PS5_NATIVE_FOUNDATION` to the prepared foundation, `PW_FOUNDATION_READY=1`, `PW_TITLE_ID=FAKE78000`, `PW_ANY_TITLE=1`, and `PW_OUTPUT_SUFFIX=-blender-shared-release`. Set `PW_LAPY_HELPER_ELF` and `PW_LAPY_HELPER_MANIFEST` to the absolute paths of the generated `owned_root_helper-FAKE78-rule/lapy.elf` and its manifest. The fallback title is not an enrollment: the managed descriptor supplies the actual title.

Run `tools/create_blender_shared_release.py --base-runtime /path/to/blender-runtime-019.2.zip --launcher /path/to/dist/FAKE78000-blender-shared-release --output /new/path/blender-runtime-019.3.zip` from the WinBrew repository root. This updates the runtime manifest, source-preparation pins, generated file table and helper policy together. Rebuild WinBrew after changing these pins. The complete corresponding-source bundle includes the helper source/build tool, launcher tree, original dependency archives, cursor patch and receipts.

## File-browser recycle and saved layouts

The launcher embeds `runtime-patches/blender/launcher/winbrew_blender_startup.py` in the launcher. `tools/generate-blender-startup.py` regenerates its C header before rebuilding. Blender file-browser menus and keyboard actions use a scoped Move to Recycle Bin operator because this Wine version's Windows `IFileOperation::DeleteItem` implementation is a stub. The operator accepts only Documents/Desktop paths, refuses links, verifies selected identities, records WinBrew's original-path metadata and renames each item into a unique recycle container. It never falls back to permanent deletion. WinBrew's recovery code accepted and restored this format in the interoperability test. The owner confirmed the actual Blender recycle action succeeds without black windows, and the original Untitled.blend opens normally.

Blender keeps the current workspace when opening a scene (`use_load_ui=False`), avoiding secondary windows saved by older projects. This changes the loaded interface policy, not the project file or scene contents. The original project is not rewritten. Saved multi-window layouts remain unsupported by the qualified display backend.
