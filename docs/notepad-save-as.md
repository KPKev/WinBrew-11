# Notepad file names and Save As

Notepad provides visible **Save** and **Save As** buttons plus File-menu commands. Ctrl+S opens Save and Ctrl+Shift+S opens Save As. The filename field is editable in both dialogs and in the **Save changes to your document?** close prompt. Click it to edit with a physical keyboard or the controller touch keyboard. A name without an extension receives `.txt`. Files are saved beside the current document; the original document stays available after Save As.

Saving a different name that already exists opens **Replace existing file?**. **Back** returns to filename editing. **Replace** uses the checked target snapshot; if the target changes or writing fails, the document stays open and the existing target is retained. An ordinary Save to the current document preserves its normal conflict checks. Read-only, unsupported or inaccessible targets cannot be silently overwritten.

Closing with edits offers **Save**, **Don't save**, and **Cancel**. Saving continues the original close/navigation only after success. Cancel keeps the edits open. Empty documents, line endings and UTF-8 BOM are preserved by the same verified atomic writer used by regular saves.
