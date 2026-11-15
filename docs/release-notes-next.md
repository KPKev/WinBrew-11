# WinBrew 11 — Recycle Bin fix

**Release:** `recycle-bin-fix-v1` · **November 14, 2026** · **Application:** PPSA77382

The Recycle Bin has been giving us trouble for a while, so I spent time getting to the bottom of it. Older deleted items could trigger access errors, files could appear buried inside extra folders, and emptying the bin wasn't reliable. This update fixes that on my PS5.

## What changed

- Deleted files show up directly in the Recycle Bin instead of being buried in extra folders.
- Restore puts a file back where it came from when its original location is recorded.
- You can permanently delete an individual item, or empty the whole bin after confirming the prompt.
- Older Recycle Bin items are recovered using checks designed to keep files in place if they can't be handled safely. The helper is restricted to WinBrew's recovery locations and user folders.
- Desktop icons can be moved by dragging, and you can drag a selection box around multiple items. In File Explorer, Triangle goes up a folder and Square goes back.

I tested the installed update on my PS5: I deleted files, found them in the bin, restored them to their original location and emptied the bin. I also ran separate PS5 tests with disposable files to check cancellation, restoring, deletion and recovery of older items. A file outside the test bin stayed untouched.

This update keeps Vapor, the existing CS2 and Blender runtimes, and the Notepad and Start menu changes from the previous release. Game compatibility can still change: Skyrim Special Edition is currently reported not working, and earlier game results aren't a guarantee for this build. See [current status](current-status.md) and [compatibility](compatibility.md).

## Download and install

Download **WinBrew-11-Recycle-Bin-Fix-v1.zip**, **SHA256SUMS.txt**, **INSTALL.md** and **fix-vapor-install-modes.py** from this release. New installations also need the unchanged [Vapor base kit](https://github.com/KPKev/WinBrew-11/releases/download/combined-blender-v1/Vapor-base-kit-0.13.zip). Save your work, close WinBrew, and back up your app and personal files before updating. The [installation guide](getting-started.md) has the full steps. No game files, accounts or saves are included.
