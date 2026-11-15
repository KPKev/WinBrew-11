# File and shortcut menus

Right-click a Desktop or Explorer file or folder. Copy and Cut select it for Paste. Paste on an empty Desktop or folder area places it there; Paste on a folder places it inside that folder. Existing items are never overwritten; copies get a new name when needed.

Folders are copied recursively. Transfers show progress and can be canceled. A failed, canceled, or changed transfer retains its original. Individual files are limited to 8 GiB; folder scans are limited to 4,096 entries and 32 levels. Links and special files are refused.

Delete moves an item to the Recycle Bin instead of permanently removing it. Open the bin to see the deleted item, then choose **Restore** to return it to its recorded original location or delete it permanently. When an older item has no recorded location, WinBrew offers to restore it to the Desktop.

To remove everything in the bin, choose **Empty Recycle Bin** and confirm the prompt. Cancel leaves the items in place. If deletion is stopped after it starts, items already removed cannot be restored. WinBrew checks recovery entries before removal and keeps items it cannot safely handle. The helper is limited to WinBrew's recovery locations and user folders; it does not need access to system or game files. See [Explorer and storage](explorer-storage.md#recycle-bin).

Right-click a taskbar icon and choose **Unpin from taskbar**. The application stays installed. A running application can stay on the taskbar until closed. Right-click its Desktop shortcut to pin it again. Steam and built-in shortcut changes persist in preferences. **Remove desktop shortcut** removes only that shortcut, not the application.
