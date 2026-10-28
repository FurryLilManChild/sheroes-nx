# Checks that every tool the PC build needs is installed, and writes the result to out\logs\check_tools.log.
# Usage (from C:\sheroes-nx): powershell -ExecutionPolicy Bypass -File tools\win\check_tools.ps1

$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir 'check_tools.log'

$lines = @()
$ok = $true
function Note($name, $good, $detail) {
    $script:lines += ('{0,-6} {1,-22} {2}' -f ($(if ($good) { 'OK' } else { 'FAIL' }), $name, $detail))
    if (-not $good) { $script:ok = $false }
}

try {
    . (Join-Path $PSScriptRoot 'env.ps1')
    Note 'Visual Studio C++' $true $env:VCToolsInstallDir
} catch {
    Note 'Visual Studio C++' $false $_.Exception.Message
}

foreach ($tool in @(
        @{ n = 'clang'; c = 'clang'; a = '--version'; want = 'version 2\d\.' },
        @{ n = 'cmake'; c = 'cmake'; a = '--version'; want = 'cmake version' },
        @{ n = 'ninja'; c = 'ninja'; a = '--version'; want = '\d' },
        @{ n = 'git'; c = 'git'; a = '--version'; want = 'git version' },
        @{ n = 'python'; c = 'python'; a = '--version'; want = 'Python 3' })) {
    $cmd = Get-Command $tool.c -ErrorAction SilentlyContinue
    if (-not $cmd) { Note $tool.n $false 'not found on PATH'; continue }
    $out = (& $tool.c $tool.a 2>&1 | Select-Object -First 1) -as [string]
    Note $tool.n ($out -match $tool.want) "$out  ($($cmd.Source))"
}

# The Vulkan SDK: its installer sets VULKAN_SDK (a new PowerShell window sees it).
$vk = $env:VULKAN_SDK
if (-not $vk) {
    $vk = Get-ChildItem 'C:\VulkanSDK' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
$vkLib = if ($vk) { Join-Path $vk 'Lib\vulkan-1.lib' } else { '' }
Note 'Vulkan SDK' ($vkLib -and (Test-Path $vkLib)) $(if ($vk) { $vk } else { 'not installed: winget install --id KhronosGroup.VulkanSDK -e' })

$game = Join-Path $repo 'assets\game_root\default.xbe'
Note 'game files' (Test-Path $game) $game

$drive = (Get-Item $repo).PSDrive
$freeGb = [math]::Round($drive.Free / 1GB, 1)
Note 'free disk space' ($freeGb -ge 10) "$freeGb GB free on $($drive.Name):"

$lines += ''
$lines += $(if ($ok) { 'ALL GOOD: ready to build.' } else { 'Something is missing: see the FAIL lines above.' })
$lines | Set-Content -Path $log -Encoding utf8
$lines | ForEach-Object { Write-Host $_ }
