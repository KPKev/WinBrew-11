# Making Blender work through Vapor without per-user repairs

November 16, 2026 · WinBrew 11 / Vapor · Blender Steam application 365670

This release turns the repairs developed on the owner's PS5 into managed installation and upgrade behavior. On a correctly installed WinBrew system with the normal console payloads running, the intended workflow is **open Vapor → find Blender → download/install → Play**. Preparation selects the matched runtime, installs its launcher and helper, connects Documents/Desktop, and applies the Blender interface compatibility policy. Users do not need to enter Python commands, rename a title folder, copy project files between two profiles, or repeat the development repairs.

Existing installations should open Blender through Vapor once after updating WinBrew. That gives the installer an opportunity to update the runtime and its access enrollment before using the PS5 Home tile. The same managed launcher is used by both routes. This automation does not replace the standard WinBrew/base-kit installation, jailbreak, homebrew registration or elfldr requirements.

## Why this required more than a launcher change

Several independent systems appeared to be one problem. Steam distinguishes games from software; Vapor caches account and application metadata; the installer verifies complete runtime receipts; Wine presents a virtual Windows profile; WinBrew stores personal files in a different console profile; and the PS5 renderer and filesystem have constraints that desktop Windows does not share.

A successful Blender launch therefore was not enough. It could launch while missing from the account's library, display its workspace but fail to draw a file dialog, save a project into a private Wine folder that Explorer could not see, or create a file that another process could read but could not permanently remove. Each layer needed evidence from its actual execution path. A host test or successful launch API call could not establish what was visible on the PS5.

The work repeatedly followed the same sequence: isolate the failing layer, build a narrow repair, test refusal and preservation behavior, deploy a guarded candidate, and check the normal owner workflow. Fixes that depended on the owner's exact title number or manually changed permissions had to be replaced before distribution. Later hands-on testing found two more problems—Blender's own recycle action and an older project's saved window layout—even after shared saving was working.

## 1. Steam software and account refresh

Blender is a Steam software application. The owner first confirmed that enabling Software in the PC Steam library made it visible there. Vapor still needed its own application-type and refresh correction; asking users to refresh repeatedly could not compensate for an inventory path that omitted the application.

The durable source preparation now carries the library changes into normal builds, including application metadata handling and refreshed ownership for the active account. Blender's selection is based on Steam application identity, not its display name. The owner confirmed that Blender appeared in Vapor after this repair.

## 2. Matched runtime preparation and supported upgrades

The first working launcher did not by itself solve upgrades. Existing installations could refuse Play with a message that runtime or recipe artifacts differed and a full upgrade was required. Those receipts protect installations from mixing incompatible components, so the repair did not disable verification.

The installer instead recognizes specific, known older Blender artifact sets and migrates them to the current pinned bundle. It still checks application identity, recipe identity and hashes, runtime identity, and the expected compatibility receipt. Unexpected combinations, duplicate artifacts and unrelated games are refused. The full installer tests exercise first installation, reinstallation, legacy refresh, source corruption/refusal and preservation of saved settings and project fixtures.

The release binds the Wine runtime, derived cursor/input support, launcher, helper and compatibility metadata together. Existing CS2 and other-game runtime selection is retained. No Blender application, commercial game, account or saved project is included in these runtime assets.

## 3. File dialogs, preferences and old project layouts

The owner could see Blender's workspace while Save As and Preferences opened as black secondary windows. The scoped startup policy makes those editors use Blender's main window. The existing cursor and input repairs remain in the matched runtime.

After this worked, the original `Untitled.blend` still brought back black windows while newly saved projects opened normally. The console log recorded Vulkan native-window conflicts during that original file's load. The corrective policy now keeps the current interface when loading a project rather than restoring the project's saved secondary-window layout. A persistent load handler reapplies the interface policy.

This does not rewrite the original project or discard its scene data. It intentionally does not restore saved multi-window layouts on this backend. The owner confirmed that the original project opens normally after the correction, without saving over it.

## 4. One Documents folder and one Desktop

Wine's virtual Windows user is named `prospero`. Initially, its Documents folder was separate from WinBrew's personal folders. A save could succeed yet appear missing when returning to WinBrew.

The managed launcher now maps Wine's Documents and Desktop to the authoritative folders under `/data/WinBrew/Users/Default`. It also sets the corresponding Windows shell-folder locations. The `prospero` name may still appear in a Windows-style path, but these shortcuts lead to the same stored files that WinBrew Explorer displays. They do not create an additional copy of every save.

When connecting an older prefix, the launcher retains existing private Wine folders with a `.before-WinBrew` suffix. It validates the mapping and refuses an unexpected pre-existing table instead of overwriting it. The owner confirmed saving and reopening projects in both shared folders and seeing those files in WinBrew.

## 5. Scoped filesystem access and automatic first-run setup

Sharing a path was insufficient because the two applications did not have equivalent filesystem access. Console UID0 is not a universal bypass for every UID1-owned file in this environment. Permissions had to work for file creation, overwrite-by-rename, reopening and later deletion.

The selected design uses a dedicated filesystem group for Documents and Desktop. It preserves native filesystem behavior instead of copying files through a second storage system. The helper validates the canonical profile marker and directory ancestry before granting access. It preserves file contents and owners, refuses unsupported metadata, does not follow symbolic links, and journals permission changes in memory so an ordinary migration failure can roll them back. Traversal and allocation are bounded. Repeated setup is idempotent; an interrupted process does not rely on a content-copy transaction.

For a new WinBrew profile, the helper lets the existing verified profile initializer run first. Blender requires a valid profile before access is granted. Prepared Blender titles are admitted through a separate root-owned allow-list tied to the pinned runtime receipt. The release no longer assumes the owner's exact generated title ID or that someone already ran the manual migration.

Unrelated hidden metadata stays untouched. The known `.winbrew-trash` stores and their recovery metadata are explicitly shared inside the two approved roots. Actual PS5 fixtures tested missing-profile behavior, refusal, rollback, repeat runs, mixed ownership, hidden-file preservation and recycle metadata access. Fixtures used disposable paths rather than personal projects.

## 6. Two distinct recycle failures

The first failure was in WinBrew: a Blender-created file could be sent to the Recycle Bin but permanent deletion reported that the item could not be checked. The recycle helper needed the shared supplementary group as well. A read-only preflight isolated that permission difference; disposable PS5 deletion/refusal fixtures passed, and the owner confirmed permanent deletion through WinBrew afterward.

The second failure was inside Blender's file browser. The exact console log showed Wine's `IFileOperation::DeleteItem` function returning a stub result, followed by Blender's “Failed to prepare delete operation” error. Changing directory permissions could not implement a missing Windows API.

The Blender-only compatibility policy now redirects file-browser menu and keyboard delete actions to **Move to Recycle Bin**. After confirmation, it validates approved paths and selected identities, writes WinBrew's original-location metadata, and renames each item into a unique recycle container. It refuses links and out-of-scope paths and does not fall back to permanent deletion. If a multi-item operation fails after earlier moves, it reports the completed count; those earlier items remain recoverable.

Host tests covered content retention, folders, changed selections, linked/outside paths and collision avoidance. A separate test used WinBrew's actual recovery parser and restore implementation to restore a file created by this path. The owner then confirmed that Blender's file-browser action succeeds on PS5 without the error or black windows. The final correction does not claim that every Wine shell operation is now implemented.

## 7. Notepad and the shared-file workflow

Notepad's original 64 KiB limit was raised first to 1 MiB and then to **5 MiB (5,242,880 bytes)** at the owner's request. The larger limit also required heap-backed, deep-copy text buffers to avoid oversized stack objects and bounded undo/redo storage to control memory growth. Oversized loads preserve the already open document instead of replacing it with a partial result.

Host checks covered boundaries, atomic saves, history, copies and UI behavior. A disposable PS5 model test exercised 5 MiB load/edit/save/reopen, and the owner confirmed that the actual text file blocked by the former 1 MiB limit now opens and edits normally. This is a text-editor capacity limit, not a promise of identical responsiveness for all encodings or document structures.

## What was verified, and what was not

The repair series includes owner-confirmed library visibility, managed Vapor launch, visible Save As, shared Documents/Desktop saving and reopening, Explorer visibility, WinBrew permanent recycle deletion and editing beyond the former Notepad limit. On the final corrective build, the owner confirmed Blender recycling without black windows and normal opening of the original project. Five installed Blender title files match the final runtime hashes; the normal Vapor session confirms shared-group access and installation of the embedded startup policy.

The final app, helper and runtime bytes are packaged with manifests, checksums, corresponding source and installation instructions. The package contains 3,725 application files plus its manifest. The build-time manifest's pre-test flag is retained; the separate verification receipt records subsequent console acceptance of those bytes.

This is qualification on the owner's PS5, supported by host and disposable native fixtures. It is not certification of every firmware, a second fresh physical console, large scenes, long renders, exporters or extended sessions. Earlier save/reopen and Notepad acceptance is retained rather than described as a new run after the last layout/recycle correction. Known unrelated game regressions remain documented. See [the verification receipt](../blender-shared-verification.json), [runtime details](../blender-runtime.md) and [installation guide](../getting-started.md).
