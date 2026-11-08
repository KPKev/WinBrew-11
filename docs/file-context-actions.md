# File and shortcut menus

Right-click a Desktop or Explorer file or folder. Copy and Cut select it for Paste. Paste on an empty Desktop or folder area places it there; Paste on a folder places it inside that folder. Existing items are never overwritten; copies get a new name when needed.

Folders are copied recursively. Transfers show progress and can be canceled. A failed, canceled, or changed transfer retains its original. Individual files are limited to 8 GiB; folder scans are limited to 4,096 entries and 32 levels. Links and special files are refused.

Delete removes the item from the visible folder by moving it to a private `.winbrew-trash` recovery folder in its original parent folder. It does not permanently erase the bytes or free that storage. Desktop Recycle Bin opens the Desktop recovery folder; numbered containers hold original names. To recover an item, open its container and Copy or Cut it, then Paste into the desired folder. For other locations, enter `<original folder>/.winbrew-trash` in Explorer. Existing recovery items are not automatically purged.

To permanently remove Desktop recovery items, right-click **Recycle Bin** and choose **Empty Recycle Bin**, or open it and use the same button in Explorer. For a different folder, right-click its empty area and choose **Empty folder Recycle Bin**. The confirmation shows the number of files/folders and file-data bytes; **Cancel** is selected by default. **Delete forever** starts permanent removal. **Stop** keeps the items not yet removed; already deleted items cannot be restored. The recovery root remains as an empty folder. Link/reparse paths, changed entries and inaccessible items are refused. One checked batch is limited to 65,536 entries and 64 levels.

Right-click a taskbar icon and choose **Unpin from taskbar**. The application stays installed. A running application can stay on the taskbar until closed. Right-click its Desktop shortcut to pin it again. Steam and built-in shortcut changes persist in preferences. **Remove desktop shortcut** removes only that shortcut, not the application.
