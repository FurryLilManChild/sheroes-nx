# Traces the next frame the game draws: one line per draw in the game's log ([FTRACE] lines), for Claude.
# Use while the game is running (started with play.ps1 or boot_test.ps1), from a second PowerShell window:
#   powershell -ExecutionPolicy Bypass -File C:\sheroes-nx\tools\win\trace_frame.ps1
$flag = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path 'out\ftrace.flag'
New-Item -ItemType File -Force -Path $flag | Out-Null
Write-Host 'Done: the next frame is traced into the game log.'
