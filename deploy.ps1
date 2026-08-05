<#
.SYNOPSIS
    windeployqt packaging script - produces a portable (green) folder.

.DESCRIPTION
    The script is intentionally kept ASCII-only so that it parses correctly no
    matter which console code page PowerShell starts with.

.EXAMPLE
    .\deploy.ps1
    .\deploy.ps1 -Config Release
#>

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [string] $QtDir = '',
    [string] $BuildDir = '',
    [string] $OutDir = ''
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

function Write-Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }
function Write-Ok($text)   { Write-Host "    $text" -ForegroundColor Green }

# ---------------------------------------------------------------------------
# 1. Locate Qt
# ---------------------------------------------------------------------------
Write-Step 'Locating Qt6'
if (-not $QtDir) {
    $candidates = @(
        'D:\Qt\6.11.1\msvc2022_64',
        'D:\Qt\6.9.3\msvc2022_64',
        'C:\Qt\6.11.1\msvc2022_64',
        'C:\Qt\6.9.3\msvc2022_64'
    )
    $QtDir = $candidates | Where-Object { Test-Path (Join-Path $_ 'bin\windeployqt.exe') } | Select-Object -First 1
}
if (-not $QtDir) { throw 'Qt6 not found. Pass -QtDir <path>.' }
$windeployqt = Join-Path $QtDir 'bin\windeployqt.exe'
if (-not (Test-Path $windeployqt)) { throw "windeployqt.exe not found: $windeployqt" }
Write-Ok $windeployqt

# ---------------------------------------------------------------------------
# 2. Locate build output (OUTPUT_NAME is localized, so probe by pattern)
# ---------------------------------------------------------------------------
Write-Step 'Locating build output'
if (-not $BuildDir) {
    $BuildDir = Join-Path $root ('build\ninja-msvc-' + $Config.ToLower())
}
$binDir = Join-Path $BuildDir 'bin'
$exe = Get-ChildItem -Path $binDir -Filter '*.exe' -ErrorAction SilentlyContinue |
       Where-Object { $_.BaseName -notlike 'tst_*' } |
       Select-Object -First 1 -ExpandProperty FullName
if (-not $exe) {
    throw "Executable not found under $binDir. Run: .\build.ps1 -Config $Config"
}
Write-Ok $exe

# ---------------------------------------------------------------------------
# 3. Prepare output folder
# ---------------------------------------------------------------------------
if (-not $OutDir) { $OutDir = Join-Path $root 'dist\DefaultAppSetter' }
Write-Step "Creating portable folder: $OutDir"
if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

Copy-Item $exe -Destination $OutDir -Force
$targetExe = Join-Path $OutDir (Split-Path $exe -Leaf)

# ---------------------------------------------------------------------------
# 4. Run windeployqt
# ---------------------------------------------------------------------------
Write-Step 'Running windeployqt'
$env:PATH = (Join-Path $QtDir 'bin') + ';' + $env:PATH

# windeployqt needs VCINSTALLDIR to resolve --compiler-runtime. vswhere is not
# available on this machine, so probe the well known install roots directly.
if (-not $env:VCINSTALLDIR) {
    $vcRoots = @(
        'D:\Program Files\Microsoft Visual Studio\18\Community\VC',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC',
        'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC'
    )
    $vcRoot = $vcRoots | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($vcRoot) { $env:VCINSTALLDIR = $vcRoot + '\' }
}

$deployArgs = @(
    "--$($Config.ToLower())",
    '--no-opengl-sw',
    '--no-system-d3d-compiler',
    '--no-translations',
    '--no-quick-import',
    '--compiler-runtime',
    '--dir', $OutDir,
    $targetExe
)
& $windeployqt @deployArgs
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit=$LASTEXITCODE)." }

# Fallback: copy the MSVC runtime DLLs ourselves if windeployqt skipped them.
$crtNames = @('msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
              'vcruntime140.dll', 'vcruntime140_1.dll', 'concrt140.dll')
$missing = $crtNames | Where-Object { -not (Test-Path (Join-Path $OutDir $_)) }
if ($missing -and $env:VCINSTALLDIR) {
    $redistRoot = Join-Path $env:VCINSTALLDIR 'Redist\MSVC'
    if (Test-Path $redistRoot) {
        $crtDir = Get-ChildItem $redistRoot -Directory -ErrorAction SilentlyContinue |
                  Sort-Object Name -Descending |
                  ForEach-Object { Get-ChildItem (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue } |
                  Select-Object -First 1
        if ($crtDir) {
            foreach ($name in $crtNames) {
                $src = Join-Path $crtDir.FullName $name
                if (Test-Path $src) { Copy-Item $src -Destination $OutDir -Force }
            }
            Write-Ok "MSVC runtime copied from $($crtDir.FullName)"
        }
    }
}
$stillMissing = @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') |
                Where-Object { -not (Test-Path (Join-Path $OutDir $_)) }
if ($stillMissing) {
    Write-Host "    Warning: MSVC runtime not bundled ($($stillMissing -join ', ')). Target machine needs VC++ Redistributable." -ForegroundColor Yellow
} else {
    # The runtime DLLs are already side by side, so the ~25 MB redist installer
    # windeployqt may have copied is dead weight for a portable folder.
    Get-ChildItem $OutDir -Filter 'vc_redist*.exe' -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

# --no-translations skips every .qm file, so copy the zh_CN ones back manually.
$qtTransDir = Join-Path $QtDir 'translations'
if (Test-Path $qtTransDir) {
    $destTrans = Join-Path $OutDir 'translations'
    New-Item -ItemType Directory -Path $destTrans -Force | Out-Null
    Get-ChildItem $qtTransDir -Filter 'qt*_zh_CN.qm' -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item $_.FullName -Destination $destTrans -Force }
    Write-Ok 'zh_CN translations bundled'
}

# ---------------------------------------------------------------------------
# 5. qt.conf + README
# ---------------------------------------------------------------------------
Write-Step 'Writing qt.conf'
@"
[Paths]
Prefix = .
Plugins = plugins
Translations = translations
"@ | Set-Content -Path (Join-Path $OutDir 'qt.conf') -Encoding UTF8

$readme = Join-Path $root 'README.md'
if (Test-Path $readme) { Copy-Item $readme -Destination $OutDir -Force }

$size = (Get-ChildItem $OutDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host "`nPackaging done: $OutDir  (~$([math]::Round($size,1)) MB)" -ForegroundColor Green
Write-Host 'Copy the whole folder to any Windows 11 machine and run it.' -ForegroundColor Green
