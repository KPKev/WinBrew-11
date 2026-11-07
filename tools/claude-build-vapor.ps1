# Build a Vapor test update with the standard pipeline and install it with the guarded updater.
# Written by Claude for KPKev. Usage (PowerShell, repo root):
#   powershell -ExecutionPolicy Bypass -File tools\claude-build-vapor.ps1 -Name vapor-controls -Old vapor-appuser2
# -Name : new build name (build/<Name>-isolated is created)
# -Old  : the build currently installed on the PS5 (the updater verifies it before changing anything)
# Stops at the first failure. Never kills processes or touches games; previous files are kept as backups.
param([Parameter(Mandatory=$true)][string]$Name, [Parameter(Mandatory=$true)][string]$Old, [switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot\..
$repo = '/mnt/c/Users/12227/OneDrive/Documents/ChatGPT/WrappedWine'
function Step($label, $cmd, $log) {
  Write-Host "`n=== $label ===" -ForegroundColor Cyan
  wsl.exe -e bash -lc "cd $repo && $cmd"
  if ($LASTEXITCODE -ne 0) {
    Write-Host "FAILED: $label (last lines of $log):" -ForegroundColor Red
    if (Test-Path $log) { Get-Content $log -Tail 40 }
    exit 1
  }
  if (Test-Path $log) { Get-Content $log -Tail 3 }
}
if (-not (Test-Path "build/$Old-isolated/PPSA77382/manifest.json")) { Write-Host "No build/$Old-isolated package; check -Old." -ForegroundColor Red; exit 1 }
if (Test-Path "build/$Name-update-status.json") { Write-Host "$Name is already built and installed (build/$Name-update-status.json). Use a new -Name for a new update." -ForegroundColor Yellow; exit 0 }
if ((Test-Path "build/$Name-review") -or (Test-Path "build/$Name-isolated")) { Write-Host "build/$Name-review or build/$Name-isolated already exists. Use a new -Name." -ForegroundColor Yellow; exit 1 }
Step '1/4 UI build'      "bash tools/build-steam-native-ui-probe.sh > build/$Name-ui.log 2>&1" "build/$Name-ui.log"
Step '2/4 Backend build' "WINE11_USE_SNAPSHOT=1 WINE11_TESTED_LINK_PROBE=1 WINE11_EXPERIMENT_GAME_HOST=1 WINE11_PROBE_DOWNLOADS=1 WINE11_PROBE_CLOUD=1 bash tools/probe-steam-link.sh > build/$Name-backend.log 2>&1" "build/$Name-backend.log"
Step '3/4 Final link'    "cp build/$Name-backend.log build/$Name-link.log && WINE11_FRESH_UI=1 WINE11_USE_SNAPSHOT=1 WINE11_PROBE_GAME_HOST=1 WINE11_PROBE_DEDICATED_HOST=1 WINE11_EXPERIMENT_GAME_HOST=1 bash tools/probe-steam-native-pie.sh >> build/$Name-link.log 2>&1" "build/$Name-link.log"
Step '4/4 Package'       "python3 tools/assemble-steam-review.py build/$Name-review build/$Name-link.log > build/$Name-package.log 2>&1 && python3 tools/assemble-steam-isolated-title.py build/$Name-isolated build/$Name-review/PPSA77381 build/$Name-link.log > build/$Name-isolated.log 2>&1" "build/$Name-isolated.log"
if ($BuildOnly) { Write-Host "`nBUILT: build/$Name-isolated (not installed)." -ForegroundColor Green; exit 0 }
Write-Host "`n=== Install (guarded updater) ===" -ForegroundColor Cyan
$py = "import sys; sys.path.insert(0,'tools'); import update_ps5_vapor_test as u; u.OLD=u.ROOT/'build/$Old-isolated/PPSA77382'; u.NEW=u.ROOT/'build/$Name-isolated/PPSA77382'; u.STATUS=u.ROOT/'build/$Name-update-status.json'; u.TAG='$Name'; "
python -c ($py + "u.stage()")
if ($LASTEXITCODE -ne 0) { Write-Host 'STAGE FAILED - nothing on the PS5 was changed. Send Claude the message above.' -ForegroundColor Red; exit 1 }
python -c ($py + "u.apply()")
if ($LASTEXITCODE -ne 0) { Write-Host 'APPLY FAILED - if it says the app is running, close Vapor/Wine 11 on the PS5 and tell Claude.' -ForegroundColor Red; exit 1 }
Write-Host "`nDONE: $Name installed. Open the Vapor test app on the PS5." -ForegroundColor Green
