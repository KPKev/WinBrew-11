# Build and install the Vapor app-user hotfix (vapor-appuser2) over the crashing vapor-appuser.
# Written by Claude for KPKev. Run from PowerShell:
#   cd C:\Users\12227\OneDrive\Documents\ChatGPT\WrappedWine
#   powershell -ExecutionPolicy Bypass -File tools\claude-vapor-appuser2.ps1
# Stops at the first failure. Never kills processes or touches games; the
# updater keeps the previous files as backups and refuses if anything is unexpected.
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot\..
$repo = '/mnt/c/Users/12227/OneDrive/Documents/ChatGPT/WrappedWine'
function Step($name, $cmd, $log) {
  Write-Host "`n=== $name ===" -ForegroundColor Cyan
  wsl.exe -e bash -lc "cd $repo && $cmd"
  if ($LASTEXITCODE -ne 0) {
    Write-Host "FAILED: $name (last lines of $log):" -ForegroundColor Red
    if (Test-Path $log) { Get-Content $log -Tail 40 }
    exit 1
  }
  if (Test-Path $log) { Get-Content $log -Tail 3 }
}
Step '1/5 UI build'      'bash tools/build-steam-native-ui-probe.sh > build/vapor-appuser2-ui.log 2>&1' 'build/vapor-appuser2-ui.log'
Step '2/5 Backend build' 'WINE11_USE_SNAPSHOT=1 WINE11_TESTED_LINK_PROBE=1 WINE11_EXPERIMENT_GAME_HOST=1 WINE11_PROBE_DOWNLOADS=1 WINE11_PROBE_CLOUD=1 bash tools/probe-steam-link.sh > build/vapor-appuser2-backend.log 2>&1' 'build/vapor-appuser2-backend.log'
Step '3/5 Final link'    'cp build/vapor-appuser2-backend.log build/vapor-appuser2-link.log && WINE11_FRESH_UI=1 WINE11_USE_SNAPSHOT=1 WINE11_PROBE_GAME_HOST=1 WINE11_PROBE_DEDICATED_HOST=1 WINE11_EXPERIMENT_GAME_HOST=1 bash tools/probe-steam-native-pie.sh >> build/vapor-appuser2-link.log 2>&1' 'build/vapor-appuser2-link.log'
Step '4/5 Package'       'python3 tools/assemble-steam-review.py build/vapor-appuser2-review build/vapor-appuser2-link.log > build/vapor-appuser2-package.log 2>&1 && python3 tools/assemble-steam-isolated-title.py build/vapor-appuser2-isolated build/vapor-appuser2-review/PPSA77381 build/vapor-appuser2-link.log > build/vapor-appuser2-isolated.log 2>&1' 'build/vapor-appuser2-isolated.log'
Step '5/5 Check fix is linked' 'grep -q gateway_syscall build/steam-native-experiment/experimental-pie.map && echo "PASS: ownership gateway linked"' 'build/vapor-appuser2-isolated.log'

Write-Host "`n=== Install (guarded updater) ===" -ForegroundColor Cyan
$py = "import sys; sys.path.insert(0,'tools'); import update_ps5_vapor_test as u; u.OLD=u.ROOT/'build/vapor-appuser-isolated/PPSA77382'; u.NEW=u.ROOT/'build/vapor-appuser2-isolated/PPSA77382'; u.STATUS=u.ROOT/'build/vapor-appuser2-update-status.json'; u.TAG='vapor-appuser2'; "
python -c ($py + "u.stage()")
if ($LASTEXITCODE -ne 0) { Write-Host 'STAGE FAILED - nothing on the PS5 was changed. Send Claude the message above.' -ForegroundColor Red; exit 1 }
python -c ($py + "u.apply()")
if ($LASTEXITCODE -ne 0) { Write-Host 'APPLY FAILED - if it says the app is running, close the error screen on the PS5 and tell Claude.' -ForegroundColor Red; exit 1 }
Write-Host "`nDONE: Vapor hotfix installed. Open the Vapor test app on the PS5." -ForegroundColor Green
