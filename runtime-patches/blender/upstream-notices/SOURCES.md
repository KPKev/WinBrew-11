# Blender compatibility runtime sources

Base runtime: Playden kit 0.19, prospero-win 7537eb0113b438e8dc0730e594fa0743203bc7c9 with Wine 490f6d5dcbb2a5047345b8af88d114bbcaad69a8. The base archive hashes and every installed file are pinned in assets/blender-runtime/manifest.json.

Derived module: win32u.prx, built with the independent hardware-cursor update patch. The base ntdll.prx is unchanged; the CS2 fixed-address patch is not applied to this Blender runtime.

Mesa provider: PS5_Mesa 4778ff0a643eb7eb45d43c412d4d3d776444060c, from the same kit. Launcher: modified ce289425bb34acbcc98f77b4e00ac98b3c16e4bc tree, with scoped Blender selection, upstream sandbox discovery and wheel forwarding. The complete modified launcher tree, module patch, pinned source archives and build scripts accompany the combined release. See runtime-patches/blender/source-receipt.json and docs/blender-runtime.md.

The runtime and source archive do not contain Blender, games, accounts, projects or saved data. The LICENSES directory retains the upstream component notices.
