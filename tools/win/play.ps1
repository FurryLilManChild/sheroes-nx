# Starts the PC build to play: no time limit, close the game window when done.
# Usage (from C:\sheroes-nx): powershell -ExecutionPolicy Bypass -File tools\win\play.ps1 [-Shots] [-Env "KEY=VALUE"]
# Log: out\logs\play_<date>_<time>.log, and out\logs\play_latest.log is always the newest.
# Controls: an Xbox or other XInput pad.

param([string[]]$Env = @(), [switch]$Shots)
& (Join-Path $PSScriptRoot 'run_game.ps1') -Kind 'play' -Seconds 0 -Env $Env -Shots:$Shots
