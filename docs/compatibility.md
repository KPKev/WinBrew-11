# Game compatibility

Updated November 7, 2026. These are reports for WinBrew 11 / Vapor on the owner's homebrew-enabled PS5, not guarantees across all game updates, consoles, or settings. Build/performance details remain unspecified when not recorded.

| Game | Evidence | Current result | Not established |
| --- | --- | --- | --- |
| [Schedule I](https://store.steampowered.com/app/3164500/Schedule_I/) | Owner's November 7 working-game report. | **Working, owner-reported.** | Exact build/settings, long-session coverage, saving, and multiplayer/co-op have not been separately characterized in this report. |
| [Counter-Strike 2](https://store.steampowered.com/app/730/CounterStrike_2/) / Steam app 730 | Owner confirmed the menu and offline gameplay with bots. | **Offline gameplay with bots works** with the matched `cs2-019.1` runtime and derived `ntdll.prx`. | LAN/local multiplayer and Internet tunneling are untested. In-game Steam authentication and official online matchmaking remain unsupported. |
| The Elder Scrolls V: Skyrim Special Edition | Owner's November 7 working-game report. | **Working offline, owner-reported.** | Specific build/settings, long-session, save/load and co-op coverage are not separately established. |
| Blackjack 21: The Blackjackiest | Owner's November 7 working-game report. | **Working offline, owner-reported.** | Specific build/settings, long-session, save/load and co-op coverage are not separately established. |
| Golf With Your Friends | Owner's November 7 working-game report. | **Working offline, owner-reported.** | Specific build/settings, long-session, save/load and co-op coverage are not separately established. |
| Quake II RTX | Owner's November 7 working-game report. | **Working offline, owner-reported.** | Specific build/settings, long-session, save/load and co-op coverage are not separately established. |
| [Blender](https://store.steampowered.com/app/365670/) | Owner confirmed stable workspace, smooth pointer/menus and responsive wheel zoom. | **Interactive input fixes confirmed.** The combined package supplies the pinned Blender runtime and launcher. | The owner confirms the earlier exit was resolved by the cursor repair. Larger-scene, long-session, render, save and export coverage is not established. |
| [Command & Conquer 3: Kane's Wrath](https://store.steampowered.com/app/24810/) | Owner's November 7 splash-screen report. | **First splash screen only; not functional gameplay.** | Menu/gameplay and sustained operation are unconfirmed. |
| [Splitgate](https://store.steampowered.com/app/677620/) | Owner's November 7 login-screen report and authentication failure. | **Blocked at server authentication.** | No gameplay confirmed; reaching login does not establish compatibility. |

“CSGO” is the owner's shorthand. The tested installation is CS2/app 730; this does not separately certify the legacy CS:GO executable.

## Network scope and priorities

Online play does not work for the listed games, according to the owner. Counter-Strike's confirmed result is **offline play**, not official online play. In-game Steam authentication and official matchmaking are unsupported and are not being pursued in this work. Vapor's Steam login supplies library/download services; it does not supply an authenticated in-game Steam client.

Co-op, LAN/local multiplayer and Internet tunneling are **deferred and untested**. The immediate priority is getting the games themselves functioning locally. No tunnel, alternate authentication service or co-op integration is included in the combined update. The owner hopes to investigate co-op later with help from other GitHub projects. No particular repository or working co-op path is selected or claimed. That future investigation is separate from these compatibility claims.

## Profiles versus working games

The package includes default profiles for Skyrim Special Edition, Terraria, Brotato, Counter-Strike: Source and Blender. Profiles select launch defaults; they do not establish owner gameplay acceptance in this release. Counter-Strike: Source/app 240 is distinct from the tested CS2/app 730 repair.

## Report a result

Open a GitHub issue with the game/app ID, game build, WinBrew release, launch route, graphics/input settings, and observed result. Distinguish an accepted launch, a visible menu, offline gameplay, LAN gameplay, and official online matchmaking. Remove account identifiers, tokens, and other private data from attachments.
