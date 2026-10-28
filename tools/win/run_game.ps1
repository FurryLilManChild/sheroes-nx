# Shared by boot_test.ps1 and play.ps1: starts out\pc\sheroes_recomp.exe on assets\game_root with the log written
# to out\logs\<kind>_<date>_<time>.log (and <kind>_latest.log), then waits up to -Seconds (0: until you close it).
#   -Env "KEY=VALUE","KEY2=VALUE2"   test switches for this run only (Claude will say which)
#   -Shots                           also save a picture every 60 frames to out\shots\ (cleared first)

param(
    [string]$Kind = 'boot',
    [int]$Seconds = 60,
    [string[]]$Env = @(),
    [switch]$Shots
)

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repo 'out\pc\sheroes_recomp.exe'
$game = Join-Path $repo 'assets\game_root'
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
if (-not (Test-Path $exe)) { Write-Host 'out\pc\sheroes_recomp.exe is missing: run tools\win\build_pc.ps1 first.'; exit 1 }

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$log = Join-Path $logDir "${Kind}_$stamp.log"

$env:NFSU2_GAME_DIR = $game
# A frame-rate line every 10 s ([fps] in the log); a -Env setting still wins.
if (-not $env:RECOMP_FPS_LOG) { $env:RECOMP_FPS_LOG = '1' }
foreach ($kv in $Env) {
    if ($kv -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2]; Write-Host "  test switch: $kv" }
}
if ($Shots) {
    $shotDir = Join-Path $repo 'out\shots'
    if (Test-Path $shotDir) { Remove-Item -Recurse -Force $shotDir }
    New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
    $env:RECOMP_GL_DUMP = (Join-Path $shotDir 'f') + ',60'
    Write-Host '  saving a picture every 60 frames to out\shots\'
}

# cmd.exe merges the program's stdout and stderr into one log, in order.
$cmdExe = if ($env:ComSpec) { $env:ComSpec } else { Join-Path $env:SystemRoot 'System32\cmd.exe' }
$line = "`"`"$exe`" > `"$log`" 2>&1`""
if ($Seconds -gt 0) { Write-Host "Starting Sonic Heroes for $Seconds seconds..." } else { Write-Host 'Starting Sonic Heroes (close its window to stop)...' }
$p = Start-Process -FilePath $cmdExe -ArgumentList '/c', $line -WorkingDirectory (Split-Path $exe) -PassThru -WindowStyle Minimized

$exited = if ($Seconds -gt 0) { $p.WaitForExit($Seconds * 1000) } else { $p.WaitForExit(); $true }
$game_proc = Get-Process -Name 'sheroes_recomp' -ErrorAction SilentlyContinue
if ($game_proc) {
    $game_proc | Stop-Process -Force -ErrorAction SilentlyContinue
    $result = "Still running after $Seconds s: closed it."
} elseif ($Seconds -gt 0 -and $exited) {
    $result = "The game closed by itself in under $Seconds s (a crash, most likely)."
} else {
    $result = 'The game was closed.'
}
Start-Sleep -Milliseconds 500
Add-Content -Path $log -Value "`n[$Kind] $result"
Copy-Item $log (Join-Path $logDir "${Kind}_latest.log") -Force
Write-Host $result
Write-Host "Log: out\logs\${Kind}_$stamp.log (Claude reads it from the connected folder)."
