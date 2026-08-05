<#
.SYNOPSIS
    DefaultAppSetter - one-click build script (MSVC + Ninja + Qt6)

.DESCRIPTION
    Imports vcvars64.bat, then configures and builds with CMake.
    cmake/ninja are not on PATH on this machine, so absolute Qt-bundled paths are used.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Config Debug
    .\build.ps1 -Clean -Deploy
#>

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [string] $QtDir = '',
    [string] $VsDir = '',

    [switch] $Clean,
    [switch] $Deploy,
    [switch] $RunTests,

    # Skip the CMake configure step when build.ninja already exists (faster
    # incremental rebuild; also works around occasional configure crashes).
    [switch] $NoConfigure
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

function Write-Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }
function Write-Ok($text)   { Write-Host "    $text" -ForegroundColor Green }
function Write-Warn2($text){ Write-Host "    $text" -ForegroundColor Yellow }

# ---------------------------------------------------------------------------
# 1. Locate Qt
# ---------------------------------------------------------------------------
Write-Step 'Locating Qt6 (msvc2022_64)'
if (-not $QtDir) {
    $candidates = @(
        'D:\Qt\6.11.1\msvc2022_64',
        'D:\Qt\6.9.3\msvc2022_64',
        'C:\Qt\6.11.1\msvc2022_64',
        'C:\Qt\6.9.3\msvc2022_64'
    )
    $QtDir = $candidates | Where-Object { Test-Path (Join-Path $_ 'lib\cmake\Qt6\Qt6Config.cmake') } | Select-Object -First 1
}
if (-not $QtDir -or -not (Test-Path $QtDir)) {
    throw 'Qt6 msvc2022_64 not found. Use -QtDir to specify.'
}
Write-Ok $QtDir

# ---------------------------------------------------------------------------
# 2. Locate CMake / Ninja
# ---------------------------------------------------------------------------
Write-Step 'Locating CMake and Ninja'
$qtRoot = Split-Path (Split-Path $QtDir -Parent) -Parent
$cmakeCandidates = @(
    (Join-Path $qtRoot 'Tools\CMake_64\bin\cmake.exe'),
    'D:\Qt\Tools\CMake_64\bin\cmake.exe',
    'C:\Qt\Tools\CMake_64\bin\cmake.exe'
)
$cmake = $cmakeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cmake) {
    $cmd = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmd) { $cmake = $cmd.Source }
}
if (-not $cmake) { throw 'cmake.exe not found.' }

$ninjaCandidates = @(
    (Join-Path $qtRoot 'Tools\Ninja\ninja.exe'),
    'D:\Qt\Tools\Ninja\ninja.exe',
    'C:\Qt\Tools\Ninja\ninja.exe'
)
$ninja = $ninjaCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $ninja) {
    $cmd = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if ($cmd) { $ninja = $cmd.Source }
}
if (-not $ninja) { throw 'ninja.exe not found.' }
Write-Ok $cmake
Write-Ok $ninja

# ---------------------------------------------------------------------------
# 3. Import MSVC environment (no vswhere.exe; enumerate vcvars64.bat)
# ---------------------------------------------------------------------------
Write-Step 'Importing MSVC environment'
$vcvars = $null
$vcvarsCandidates = @(
    'D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat',
    'D:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat'
)
if ($VsDir) {
    $vcvarsCandidates = @(Join-Path $VsDir 'VC\Auxiliary\Build\vcvars64.bat') + $vcvarsCandidates
}
foreach ($c in $vcvarsCandidates) {
    if (Test-Path $c) { $vcvars = $c; break }
}
if (-not $vcvars) {
    $vsRoots = @(
        'D:\Program Files\Microsoft Visual Studio',
        'C:\Program Files\Microsoft Visual Studio',
        'D:\Program Files (x86)\Microsoft Visual Studio',
        'C:\Program Files (x86)\Microsoft Visual Studio'
    ) | Where-Object { Test-Path $_ }

    foreach ($vsRoot in $vsRoots) {
        $found = Get-ChildItem -Path $vsRoot -Recurse -Depth 8 -Filter 'vcvars64.bat' -ErrorAction SilentlyContinue |
                 Sort-Object FullName -Descending | Select-Object -First 1
        if ($found) { $vcvars = $found.FullName; break }
    }
}
if (-not $vcvars) { throw 'vcvars64.bat not found. Use -VsDir to specify VS install dir.' }
Write-Ok $vcvars

$envDump = & cmd.exe /c "`"$vcvars`" >nul 2>&1 && set"
if ($LASTEXITCODE -ne 0) { throw "Failed to import MSVC env (exit=$LASTEXITCODE)." }
foreach ($line in $envDump) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2] -ErrorAction SilentlyContinue
    }
}
Write-Ok "cl.exe -> $((Get-Command cl.exe -ErrorAction SilentlyContinue).Source)"

# Add Qt and Ninja to PATH for windeployqt / ninja
$env:PATH = (Join-Path $QtDir 'bin') + ';' + (Split-Path $ninja -Parent) + ';' + $env:PATH

# ---------------------------------------------------------------------------
# 4. Configure and build
# ---------------------------------------------------------------------------
$buildDir = Join-Path $root ("build\ninja-msvc-" + $Config.ToLower())
if ($Clean -and (Test-Path $buildDir)) {
    Write-Step "Cleaning $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

$ninjaFile = Join-Path $buildDir 'build.ninja'
if ($NoConfigure -and (Test-Path $ninjaFile)) {
    Write-Step 'Skipping CMake configure (-NoConfigure)'
} else {
    Write-Step "CMake configure ($Config)"
    & $cmake -S $root -B $buildDir -G Ninja `
        "-DCMAKE_BUILD_TYPE=$Config" `
        "-DCMAKE_PREFIX_PATH=$QtDir" `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        '-DCMAKE_C_COMPILER=cl.exe' `
        '-DCMAKE_CXX_COMPILER=cl.exe'
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit=$LASTEXITCODE)." }
}

Write-Step 'Building'
& $cmake --build $buildDir --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit=$LASTEXITCODE)." }

# OUTPUT_NAME is a localized (non-ASCII) name, so probe the bin folder instead
# of hard-coding the file name.
$binDir = Join-Path $buildDir 'bin'
$exe = Get-ChildItem -Path $binDir -Filter '*.exe' -ErrorAction SilentlyContinue |
       Where-Object { $_.BaseName -notlike 'tst_*' } |
       Select-Object -First 1 -ExpandProperty FullName
if ($exe) {
    Write-Ok "Built: $exe"
} else {
    Write-Warn2 'Executable not found at expected path; check build output.'
}

# ---------------------------------------------------------------------------
# 5. Optional: unit tests
# ---------------------------------------------------------------------------
if ($RunTests) {
    Write-Step 'Running unit tests'
    $ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
    & $ctest --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { Write-Warn2 "Some tests failed (exit=$LASTEXITCODE)." }
}

# ---------------------------------------------------------------------------
# 6. Optional: deploy
# ---------------------------------------------------------------------------
if ($Deploy) {
    & (Join-Path $root 'deploy.ps1') -Config $Config -QtDir $QtDir -BuildDir $buildDir
    if ($LASTEXITCODE -ne 0) { throw 'Deploy failed.' }
}

Write-Host "`nBuild complete." -ForegroundColor Green
