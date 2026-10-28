# Starts the PC build, lets it run for a while (default 60 s), then closes it and keeps the log.
# Usage (from C:\sheroes-nx): powershell -ExecutionPolicy Bypass -File tools\win\boot_test.ps1 [-Seconds 60] [-Shots] [-Env "KEY=VALUE"]
# The game window opens; you can watch it (and play if it gets that far). It is closed after -Seconds.
# Log: out\logs\boot_<date>_<time>.log, and out\logs\boot_latest.log is always the newest.

param([int]$Seconds = 60, [string[]]$Env = @(), [switch]$Shots)
& (Join-Path $PSScriptRoot 'run_game.ps1') -Kind 'boot' -Seconds $Seconds -Env $Env -Shots:$Shots
