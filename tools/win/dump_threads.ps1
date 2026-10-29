# While the game is running (started with play.ps1 or boot_test.ps1), from a second PowerShell window:
#   powershell -ExecutionPolicy Bypass -File C:\sheroes-nx\tools\win\dump_threads.ps1
# The game writes what every thread is doing into its log ([THREADS] lines), for Claude. Use it when the game
# seems stuck (a loading screen that never ends, a freeze).
$flag = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path 'out\threads.flag'
New-Item -ItemType File -Force -Path $flag | Out-Null
Write-Host 'Done: the game writes its threads into the log within a second.'
