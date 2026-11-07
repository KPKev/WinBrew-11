# Temporary download deletion guard

The normal Wine 11 Steam bridge temporarily disables cancellation and game-file removal. Playden's current partial-download remover has unresolved path and identity safety issues, so the production coordinator refuses cancellation, the generated service refuses direct cancel and uninstall calls, and the guarded runner refuses removal, including an uninstall left pending from an earlier session. The Downloads screen hides Cancel and says that pausing keeps the files. Pause and Resume remain available.

This is a temporary fail-closed restriction, not cancellation, cleanup, or crash recovery. Partial files can remain on storage. Re-enable deletion only after the replacement remover and recovery path have been independently reviewed and tested for the target environment.
