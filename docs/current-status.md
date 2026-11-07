# Current verification — November 7, 2026

The combined WinBrew 11 / Vapor application is installed on the owner's PS5 as **PPSA77382**, version **01.016.000**. Its full 1,856-file package was checked after installation. Startup reached Steam-ready state and initialized keyboard, mouse, and controller input. The owner will perform further hands-on testing of the combined update.

## Gameplay reports

- **Schedule I:** working, reported by the owner. This report does not establish a particular game build, performance figure, or co-op result.
- **Counter-Strike 2 / Steam app 730:** offline gameplay with bots confirmed by the owner using the fixed runtime. In-game Steam authentication and official online matchmaking remain unsupported. LAN/local play may work but has not been tested; Internet tunneling has not been tested. Legacy CS:GO was not separately tested.
- **The Elder Scrolls V: Skyrim Special Edition:** working, owner-reported; online unavailable and co-op untested.
- **Blackjack 21: The Blackjackiest:** working, owner-reported; online unavailable and co-op untested.
- **Golf With Your Friends:** working, owner-reported; online unavailable and co-op untested.
- **Quake II RTX:** working, owner-reported; online unavailable and co-op untested.
- The owner confirmed **“Smooth now”** after the Vapor window-shadow fix. Further combined optimizations are installed; startup verification is separate from hands-on acceptance of every feature.

See [compatibility](compatibility.md) for limits and research links.

## Prepared Blender and game-status update

The next combined release adds the [Blender compatibility bundle](blender-runtime.md) to normal installation and reinstallation. The owner's repaired copy confirmed stable workspace visibility, smooth pointer/menus and responsive wheel zoom. The owner confirms the earlier automatic exit was resolved by the cursor repair; large-scene/render/export coverage remains separate. Release delivery verification is recorded separately from console acceptance of the owner's per-title fixes.

Kane's Wrath currently reaches only the first splash screen. Splitgate reaches login but cannot proceed through server authentication. Neither is listed as working gameplay. Counter-Strike remains confirmed for offline play; official online play is unsupported and is not being pursued. Online is unavailable for the listed games. Co-op may be researched later with other GitHub projects, but co-op/multiplayer work is deferred while the priority is making games function locally.

See the [prepared release notes](release-notes-next.md). No later package has been published or installed by this release-preparation task.

## Cache behavior

Successful per-game artwork, description/genres, and news snapshots persist across restarts. Cached presentation content is reused; the **Refresh game cache** action clears that game's presentation cache and requests fresh content. Automatic game discovery remains enabled. Ownership, install state, download progress, and launch eligibility continue to use live state. Tests cover persistence, offline reuse, reset, corrupt entries, and in-flight requests.

## Efficiency work

The shadow renderer skips the hidden window interior instead of blending it 18 times per frame. Glyph coverage is precomputed; Recent Games ordering and shortcut lookups are reused; download views are copied when the worker publishes a change. Artwork workers sleep on a condition variable, and native rendering is paced by display synchronization. Reviewed reconnect checks remain cadenced rather than running as unbounded idle polls.

Local synthetic measurements with a 1,000-game library illustrate the rendering improvement:

| Host benchmark stage | Home render | Detail render |
| --- | ---: | ---: |
| Before shadow fix | 106.6 ms | 104.8 ms |
| After shadow fix, separate run | 9.1 ms | 7.4 ms |
| Further efficiency audit | 7.5 ms | 5.8 ms |

These are local host measurements from separate runs, **not console FPS or a controlled PS5 benchmark**. Parallel background work is retained where useful; avoiding repeated work and expensive drawing resolved the observed pointer issue.

## Checks and installed identity

Regression checks passed for 10 pixel-exact shadow cases, 54 text cases across characters/sizes/clipping/colors, input and controller behavior, discovery/Recent ordering and shortcut call counts, downloads and publication generations, maintenance/freeze behavior, and desktop applications. Fixed-buffer rendering checks found no array allocations in the exercised paths. The full native application compiled and packaged successfully.

The runtime installer tests cover the complete 1,760-file CS2 runtime, first install/reinstall, other Steam/local selection, corrupt/missing/linked files, and migration preserving a saved file. These filesystem fixtures do not imply another complete game download on the owner's console.

Installed package verification covered all 1,760 runtime files, 49 runtime notices, the remaining 47 application files, and the final manifest. The existing working Counter-Strike launcher and derived runtime module remained unchanged. No game reinstall or account/save mutation was performed for this app update.

- Application executable SHA256: `2d322963599fc2d159a7458254a5a674f791cdbd1813e196c0bcc8c851b2736c`
- Installed manifest SHA256: `933305eb4f6e1e2edb034bc92cbb6dfbc48136fb3ac0665ddefd9756382379be`
- Derived CS2 `ntdll.prx` SHA256: `4f9f9fd45c65be0320794186f94bc2611be0d223199955a06d1e984cd295d94b`

[Install the release](getting-started.md), [build from source](source-build.md), or inspect the [machine-readable receipt](current-verification.json).
