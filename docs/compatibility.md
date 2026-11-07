# Game compatibility

Updated November 7, 2026. These are reports for WinBrew 11 / Vapor on the owner's homebrew-enabled PS5, not guarantees across all game updates, consoles, or settings. Build/performance details remain unspecified when not recorded.

| Game | Evidence | Current result | Not established |
| --- | --- | --- | --- |
| [Schedule I](https://store.steampowered.com/app/3164500/Schedule_I/) | Owner's November 7 working-game report. | **Working, owner-reported.** | Exact build/settings, long-session coverage, saving, and multiplayer/co-op have not been separately characterized in this report. |
| [Counter-Strike 2](https://store.steampowered.com/app/730/CounterStrike_2/) / Steam app 730 | Owner confirmed the menu and offline gameplay with bots. | **Offline gameplay with bots works** with the matched `cs2-019.1` runtime and derived `ntdll.prx`. | LAN/local multiplayer and Internet tunneling are untested. In-game Steam authentication and official online matchmaking remain unsupported. |

“CSGO” is the owner's shorthand. The tested installation is CS2/app 730; this does not separately certify the legacy CS:GO executable.

## Local play and Internet tunneling

Local/LAN multiplayer **may work, but the owner has not tried it**. Working offline bots is not a successful multiplayer test. Vapor's Steam login supplies library/download services, not an authenticated Steam client inside the game.

Open-source LAN transport/bridging projects worth researching include [switch-lan-play](https://github.com/spacemeowx2/switch-lan-play) and [SoftEtherVPN](https://github.com/SoftEtherVPN/SoftEtherVPN). These are research references, not compatibility recommendations: neither is bundled, integrated, configured, or tested with this WinBrew/CS2 setup. A tunnel alone does not establish that this game's LAN discovery, connection path, or authentication works through the PS5 runtime, and it does not supply official Steam matchmaking.

A future multiplayer report should record the game builds, host/client platforms, runtime, network/tunnel project and version, whether a match actually starts, and sustained gameplay. Until then, LAN and Internet tunneling remain **untested**.

## Profiles versus working games

The package includes default profiles for Skyrim Special Edition, Terraria, Brotato, and Counter-Strike: Source. Profiles select launch defaults; they do not establish owner gameplay acceptance in this release. Counter-Strike: Source/app 240 is distinct from the tested CS2/app 730 repair.

## Report a result

Open a GitHub issue with the game/app ID, game build, WinBrew release, launch route, graphics/input settings, and observed result. Distinguish an accepted launch, a visible menu, offline gameplay, LAN gameplay, and official online matchmaking. Remove account identifiers, tokens, and other private data from attachments.
