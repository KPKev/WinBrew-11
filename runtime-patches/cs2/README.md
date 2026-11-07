# Counter-Strike PS5 fixed-hint runtime repair

This is a derived ntdll.prx from Playden kit 0.19, not an upstream release.
No other runtime module, Windows PE DLL, game file or bottle is replaced.

The console accepted a fixed virtual reservation at 52 GiB but refused both
CPU and GPU direct-memory mappings there with EINVAL. The same signed app's
192 GiB control succeeded. V8 reserved a randomized address in the PS5 system
32–64 GiB band, then its commit failed. Rejecting that fixed guest hint before
kernel reservation lets the caller use its existing ordinary high-area retry.

Changes: overflow-safe range helper rejects spans intersecting
[0x800000000,0x1000000000), retaining valid low and high hints. The fixed-address
path also verifies the kernel returned the requested address and releases only
a newly owned mismatched returned reservation. Host module mappings bypass
this Wine guest allocation path.

Inputs and licenses: Wine commit 490f6d5dcbb2a5047345b8af88d114bbcaad69a8;
prospero-win 7537eb0113b438e8dc0730e594fa0743203bc7c9, all 147 patches applied;
module-export foundation 30597512539e7edfde079cbcaf4a626bc0a948c5. Their complete
source archives, licenses and hashes are included. The extra helper is LGPL-2.1-or-later.
SDK 0.42 and LLVM 18.1.8 are external build prerequisites, not bundled.

Reproduction: download cs2-runtime-corresponding-source.zip from the
cs2-runtime-v1 GitHub release. On Linux/WSL, stage a NEW workspace with:

```
python3 tools/prepare-cs2-rebuild.py cs2-runtime-corresponding-source.zip /tmp/cs2-rebuild --sdk /absolute/path/to/ps5-payload-sdk
```

This checks the complete bundle digest, stages the original sources/scripts
and links the external SDK without modifying it. Follow the printed commands
from the new workspace. Use a fresh ~/counterstrike-wine019-work and cached
LLVM 18.1.8 at ~/.cache/wine11-llvm-18.1.8/root/usr. The included generated
configuration is reference evidence; the scripts configure a fresh build.
 The supplied prepare-source.py extracts the pinned
archives from evidence/cs2-source-archives.json into a new WSL directory
~/counterstrike-wine019-work, applies the exact upstream patches, and stages
Vulkan, clock, title-menu and shared-input ABI headers. prepare-tools.sh fetches
Ubuntu's m4/bison/flex packages into that isolated directory and extracts them,
without installing system packages. build-tool-and-test.sh compiles the
module-export tool and optimized/sanitized boundary tests. build-host-tools.sh
configures host Wine and builds only __tooldeps__. apply-fix.py applies the
local header and virtual.c changes. build-ntdll-unix.sh configures a PS5 cross
build and compiles only dlls/ntdll/ntdll.so, heap, direct-memory and section
policy objects. link-ntdll-prx.sh uses the exact release descriptor/shim list,
then converts/signs ntdll.prx. audit-ntdll.py checks native imports, exports,
SELF integrity and unexpected raw syscall instructions. Preserve source path
structure or adjust the documented paths when reproducing outside this repo.

FreeType, TLS, media, GPU drivers and other Windows DLLs are intentionally not
rebuilt: ntdll does not require those optional build inputs. Existing matched
kit0.19 modules remain installed. Corresponding generated config and logs are
included. Header-staging and missing-sections errors during initial setup were
corrected before the final link; current audit has no unresolved symbol.

Validation: boundary, captured failing-address, spanning/overlap and overflow
cases pass optimized GCC and ASan+UBSan. Native audit reports no unresolved,
title-unbound, WebKit-unbound, missing required exports or raw syscalls.
SELF integrity is valid. These checks do not establish console gameplay.

Deployment/rollback: stop only the matching title before modification. Verify
the existing ntdll.prx hash ac249ad6d13e9818f7a28f0ba0c2b489df31064610c91f13ea20cee6194dfbe5,
stage and read back the candidate, then preserve that original by same-parent
rename. Keep the original launcher and all other kit0.19 files. If needed,
restore the exact original PRX after the title stops. No console action is
performed by these build scripts.


Automatic delivery in Vapor uses the complete matched 0.19 Wine directory,
not only this one module. Its inventory is pinned separately in
assets/cs2-runtime/manifest.json. See docs/cs2-runtime.md. The owner confirmed
offline gameplay with bots; in-game online Steam authentication is unsupported.
