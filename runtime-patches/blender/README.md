# Blender compatibility source

The source receipt, cursor patch, complete launcher archive and build scripts correspond to the prepared `blender-019.1` compatibility bundle. See [the runtime guide](../../docs/blender-runtime.md) for build prerequisites, exact qualification scope and installation policy.

`launcher-source.tar.gz` contains the complete modified ce28942-based launcher tree and its LGPL/component notices. The patch applies to the Wine tree after the complete prospero-win 7537eb0 patch series. The prepared corresponding-source release ZIP supplies the pinned upstream archives and FreeType source as well.

The independent-cursor contract test covers motion without a graphics present, hiding, and GDI fallback. The release launcher test is `tests/blender_launcher_test.c`; the complete installer test is `tools/test-blender-runtime-installer.sh`. No console or personal data is part of this source bundle.
