@echo off
setlocal
cd /d "%~dp0.."
if not exist build mkdir build
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /O2 /EHsc /W3 src\shell.cpp src\vapor_shortcuts.cpp src\desktop_ui.cpp src\preferences.cpp src\pointer.cpp src\controller_input.cpp src\explorer.cpp src\explorer_fileops.cpp src\privilege.cpp src\explorer_context.cpp src\notepad_document.cpp src\notepad_ui.cpp src\paint.cpp src\image_codec.cpp src\game_inventory.cpp src\web_history.cpp src\tray_ui.cpp src\icons.cpp src\text_input.cpp src\keyboard_ui.cpp src\hid_input.cpp src\host.cpp /Fe:build\WinBrew11-Preview.exe /Fo:build\ user32.lib gdi32.lib shell32.lib
exit /b %errorlevel%
