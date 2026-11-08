# Built-in Notepad, Paint and Spreadsheet

These are WinBrew applications with familiar desktop layouts. They are not Microsoft Office installations or complete replicas of Notepad, Paint or Excel.

## Notepad

Text editing supports mouse, USB keyboard and controller input, multiline editing, visible Save/Save As and an editable filename in save/close prompts. A missing extension receives `.txt`. Replacing a different existing file requires confirmation. Changed targets and failed saves retain document edits. The atomic writer preserves supported line endings and UTF-8 BOM. See [Save As details](notepad-save-as.md).

## Paint

Tools include brush/pencil, eraser, line, rectangle, ellipse and color picker, plus undo/redo history. Image opening supports PNG, JPEG and BMP; saving uses the bounded PNG writer. Canvas and history are bounded for console memory. This is not a layered professional image editor or support for arbitrary image formats.

## Spreadsheet / Excel-style workbook support

The editor opens/saves CSV and supported `.xlsx` SpreadsheetML ZIP packages. It switches worksheets and edits cell values/formulas in the visible A1:Z100 grid. Local calculation supports basic arithmetic/references and SUM, AVERAGE, MIN, MAX and COUNT; it is not Excel's complete calculation engine.

Unedited XLSX package parts and cell XML are retained so content outside the displayed grid or editor is preserved instead of silently flattened. Interoperability fixtures cover multiple sheets, shared/inline strings, formulas, style/chart parts, protected/array cells, literal leading equals signs and content beyond the visible grid. Preserving a chart/style or unsupported formula is not rendering, editing or correctly calculating it. Some unsupported or oversized cells remain read-only. Macros are not executed, legacy `.xls` is unsupported, and full formatting/chart/Excel parity is not claimed.

Import limits are 32 MiB compressed archive, 8 MiB per part and 16 MiB total expanded content. Encrypted/malformed/unsafe ZIP/XML is refused. Save failures and external-file conflicts preserve the existing file. Keep a separate backup of important workbooks and check results in Excel or another compatible application.

## Creating files

Right-click New or use Explorer to create folders, text documents, PNG images, XLSX workbooks and CSV spreadsheets. Personal defaults use the new Desktop under `C:\WinBrew\Users\Default`. New in another open folder uses that folder. First-profile migration retains original personal files.

## Verification limits

Editor, conflict and workbook interoperability checks passed on the host, and the complete native app was built/installed. The owner confirmed Notepad opening/saving and the desktop/file-manager workflow. Every advanced spreadsheet operation has not been physically exercised on PS5; automated round-trip checks do not establish full Excel compatibility.
