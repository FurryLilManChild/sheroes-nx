# Builds the Windows PC test build of sheroes-nx: the same Vulkan renderer as the Switch (nv2a_vk), so a PC test
# exercises the Switch's code with a real GPU. Every step's full output goes to out\logs\.
#
# Usage (from C:\sheroes-nx): powershell -ExecutionPolicy Bypass -File tools\win\build_pc.ps1 [-Jobs N] [-Clean] [-Regen]
#   -Jobs   parallel compile jobs (default: half the logical CPUs, at most 8; each lifted file needs ~1-2 GB RAM)
#   -Clean  delete the build folder first
#   -Regen  translate the game code again even if nothing changed
#
# Steps: the libraries (first time: SDL2's Visual C++ package, glslang 15.4.0 built from source), the code
# translation (XBE to C with the vendored xboxrecomp tools, from your own disc: nothing game-derived is downloaded or
# committed), configure, compile. The translation runs again by itself when config\seed_functions.json or
# src\recomp_manual.c changed.

param(
    [int]$Jobs = 0,
    [switch]$Clean,
    [switch]$Regen
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$logDir = Join-Path $repo 'out\logs'
$deps = Join-Path $repo 'out\deps'
$gen = Join-Path $repo 'out\gen'
$build = Join-Path $repo 'out\pc'
$tk = Join-Path $repo 'xboxrecomp'
$xbe = Join-Path $repo 'assets\game_root\default.xbe'
New-Item -ItemType Directory -Force -Path $logDir, $deps | Out-Null

$sdlVer = '2.30.11'
$sdlDir = Join-Path $deps "SDL2-$sdlVer"
$glslangVer = '15.4.0'
$glslangSrc = Join-Path $deps 'glslang-src'
$glslangOut = Join-Path $deps 'glslang'

function Step($name, $logName, [scriptblock]$body) {
    $log = Join-Path $logDir $logName
    Write-Host ''
    Write-Host "== $name  (log: out\logs\$logName)"
    $start = Get-Date
    # git, cmake and python write progress to stderr; run the step with 'Continue' and turn every line into plain
    # text for the log (plain UTF-8, written as it goes).
    $sw = [System.IO.StreamWriter]::new($log, $false, [System.Text.UTF8Encoding]::new($false))
    $sw.AutoFlush = $true
    try {
        & { $ErrorActionPreference = 'Continue'; & $body } 2>&1 | ForEach-Object { "$_" } |
            ForEach-Object -Begin { $n = 0 } -Process {
                $sw.WriteLine($_)
                $n++
                # Progress: ninja's [x/y] lines every so often, and anything that looks like an error.
                if ($_ -match '^\[(\d+)/(\d+)\]') {
                    if (([int]$matches[1] % 10) -eq 0 -or $matches[1] -eq $matches[2]) { Write-Host "   $($matches[0]) steps" }
                } elseif ($_ -match 'error|FAILED') { Write-Host "   $_" }
                elseif (($n % 50) -eq 0) { Write-Host "   $_" }
            }
    } finally {
        $sw.Close()
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host ''
        Write-Host "FAILED at: $name. Tell Claude; the log is out\logs\$logName"
        exit 1
    }
    Write-Host ("   done in {0:n1} min" -f ((Get-Date) - $start).TotalMinutes)
}

. (Join-Path $PSScriptRoot 'env.ps1')

if (-not (Test-Path $xbe)) {
    Write-Host 'assets\game_root\default.xbe is missing: the extracted disc goes in assets\game_root.'
    exit 1
}
if (-not $env:VULKAN_SDK) {
    $vk = Get-ChildItem 'C:\VulkanSDK' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        Select-Object -First 1 -ExpandProperty FullName
    if ($vk) { $env:VULKAN_SDK = $vk }
    else { Write-Host 'The Vulkan SDK is missing: winget install --id KhronosGroup.VulkanSDK -e (then open a new PowerShell).'; exit 1 }
}
if ($Jobs -le 0) { $Jobs = [math]::Max(1, [math]::Min(8, [int]([Environment]::ProcessorCount / 2))) }
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

# 1. SDL2 (the renderer's window): the official Visual C++ development package.
if (-not (Test-Path (Join-Path $sdlDir 'cmake\sdl2-config.cmake'))) {
    Step "Fetching SDL2 $sdlVer (one time)" 'deps_sdl2.log' {
        $zip = Join-Path $deps 'sdl2.zip'
        Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVer/SDL2-devel-$sdlVer-VC.zip" -OutFile $zip
        Expand-Archive -Path $zip -DestinationPath $deps -Force
        Remove-Item $zip
        Write-Output "SDL2 in $sdlDir"
    }
}

# 2. glslang (the renderer compiles its shaders at run time), the version the Switch build uses.
if (-not (Test-Path (Join-Path $glslangOut 'lib\cmake\glslang\glslang-config.cmake'))) {
    Step "Building glslang $glslangVer (one time, a few minutes)" 'deps_glslang.log' {
        if (-not (Test-Path (Join-Path $glslangSrc 'CMakeLists.txt'))) {
            git clone --depth 1 --branch $glslangVer https://github.com/KhronosGroup/glslang.git $glslangSrc
        }
        cmake -S $glslangSrc -B (Join-Path $deps 'glslang-build') -G Ninja -DCMAKE_BUILD_TYPE=Release `
            -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX=$glslangOut `
            -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DENABLE_GLSLANG_BINARIES=OFF -DBUILD_SHARED_LIBS=OFF `
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
        cmake --build (Join-Path $deps 'glslang-build') -- -j $Jobs
        cmake --install (Join-Path $deps 'glslang-build')
    }
}

# 3. The code translation (XBE to C), when it has never run, when asked, or when its inputs changed: a new seed
# needs the whole pipeline, an edited override only the last step (the lift).
$seeds = Join-Path $repo 'config\seed_functions.json'
$manual = Join-Path $repo 'src\recomp_manual.c'
$stampFull = Join-Path $gen '.stamp_full'
$stampLift = Join-Path $gen '.stamp_lift'
$full = $Regen -or -not (Test-Path $stampFull) -or ((Get-Item $seeds).LastWriteTime -gt (Get-Item $stampFull).LastWriteTime)
$lift = $full -or -not (Test-Path $stampLift) -or ((Get-Item $manual).LastWriteTime -gt (Get-Item $stampLift).LastWriteTime)

if ($lift) {
    $what = if ($full) { 'Translating the game code (first time or new seeds: a few minutes)' } else { 'Re-lifting the game code (overrides changed)' }
    Step $what 'pc_regen.log' {
        python -m pip install --user --quiet --disable-pip-version-check capstone
        Push-Location $tk
        try {
            if ($full) {
                $analysis = Join-Path $repo 'assets\game_root\default_analysis.json'
                if (-not (Test-Path $analysis)) { python -m tools.xbe_parser $xbe --json $analysis --quiet }
                python -m tools.disasm $xbe --seed-functions $seeds
                if ($LASTEXITCODE -ne 0) { throw 'disasm failed' }
                python -m tools.func_id $xbe
                python -m tools.abi_analysis $xbe
            }
            # Into a fresh folder; only files whose contents changed are copied over, so the build recompiles
            # only those.
            $new = "$gen.new"
            if (Test-Path $new) { Remove-Item -Recurse -Force $new }
            New-Item -ItemType Directory -Force -Path $new, $gen | Out-Null
            python -m tools.recomp $xbe --game-name 'Sonic Heroes' --all --split 250 --gen-dir $new `
                --exclude-manual $manual --mmio-sections 'DSOUND,XPP,D3D,XGRPH'
            if ($LASTEXITCODE -ne 0) { throw 'recomp failed' }
        } finally { Pop-Location }
        $changed = 0
        foreach ($f in Get-ChildItem $new -File) {
            $dst = Join-Path $gen $f.Name
            if (-not (Test-Path $dst) -or (Get-FileHash $f.FullName).Hash -ne (Get-FileHash $dst).Hash) {
                Copy-Item $f.FullName $dst -Force; $changed++
            }
        }
        foreach ($f in Get-ChildItem $gen -File -Filter '*.*') {
            if ($f.Name -like '.stamp*') { continue }
            if (-not (Test-Path (Join-Path $new $f.Name))) { Remove-Item $f.FullName }
        }
        Remove-Item -Recurse -Force $new
        Write-Output "regen: $changed generated file(s) changed"
        $global:LASTEXITCODE = 0
    }
    if ($full) { Set-Content -Path $stampFull -Value (Get-Date -Format o) }
    Set-Content -Path $stampLift -Value (Get-Date -Format o)
}

# 4. Configure (clang with the Visual Studio libraries, as the Xbox 360 ports build).
$cfgArgs = @(
    '-S', $repo, '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++',
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',
    '-DNFSU2_VULKAN=ON',
    "-DXBOXRECOMP_DIR=$tk",
    "-DNFSU2_GEN_DIR=$gen",
    "-DSDL2_DIR=$(Join-Path $sdlDir 'cmake')",
    "-Dglslang_DIR=$(Join-Path $glslangOut 'lib\cmake\glslang')",
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
)
Step 'Configuring' 'pc_configure.log' { cmake @cfgArgs }

# 5. Compile.
Step "Compiling ($Jobs jobs; the first time takes the longest)" 'pc_build.log' { cmake --build $build -- -j $Jobs }

Copy-Item (Join-Path $sdlDir 'lib\x64\SDL2.dll') $build -Force
Write-Host ''
Write-Host 'Built: out\pc\sheroes_recomp.exe. Next: tools\win\boot_test.ps1'
