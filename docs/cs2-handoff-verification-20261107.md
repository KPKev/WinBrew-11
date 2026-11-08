# CS2 handoff hotfix — November 7, 2026

The owner confirmed **Counter-Strike opens through Vapor** after this correction. Earlier offline gameplay with bots was confirmed on the same derived ntdll. In-game Steam authentication and official online matchmaking remain unsupported.

The preparation adapter accepted an older Ready/admission-ok CS2 record before checking whether its selected runtime was current. Launch then refused the older runtime inventory. The adapter now requires the selected CS2 runtime before skipping preparation or advancing to launch. A regression test failed on the old code and passes with the fix, covering stale CS2, warm-current CS2 and unchanged other-game behavior.

The owner's previous manual runtime replacement also left 29 Wine directories owned by UID 0 while Vapor's title and marker belonged to UID 1. A read-only native probe and marker-bound dry run established the mismatch. A scoped ownership-only repair corrected those directories without file-content or mode changes. Normal Vapor preparation then refreshed the runtime inventory. Game downloads, bottles, saves, profiles and the existing patched ntdll were preserved.

Verified facts:

- Installed app: `PPSA77382`, 1,856 packaged files.
- App executable SHA256: `d5cabf3b7f986f272b8068c7e76bb859086820b0dec5edcbea4cfd45a4cc4207`.
- App manifest SHA256: `a712a3d69ad0eae2829415486053790ce38361156f4bab6442d2838e4a9b4c4c`.
- Runtime inventory after managed refresh: `f591f292f1cdd9419730c16d8a0a5052828357ab8206f327f9c6faa866239c1e`.
- Existing CS2 launcher stayed `10fc130547881d917aabf63ec1f137b04e70c215e7230607fafd364b715ec187`; ntdll stayed `4f9f9fd45c65be0320794186f94bc2611be0d223199955a06d1e984cd295d94b`.
- Native relinking retained every other Steam backend object byte-for-byte.
- All 11 installed FAKE78 titles passed read-only directory ownership/writeability/symlink checks, and each runtime inventory matched its selected baseline (default 0.13; CS2 019.1).

The audit covered Golf With Your Friends, the Quake2RTX controller test, Quake II RTX, Blackjack 21, Skyrim Special Edition, Enshrouded, Blender, CS2, Schedule I, Splitgate, and Command & Conquer 3: Kane's Wrath. It establishes absence of this runtime ownership/inventory mismatch at the time checked; it does not establish every game's gameplay compatibility or fix unrelated native/engine failures. Other game contents and profiles were not changed.
