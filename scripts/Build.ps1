<#
.SYNOPSIS
    Cross-platform Windows build script for Pelco-D Controller, Sightline SLA Suite, and libraries.

.DESCRIPTION
    Automates CMake configuration, compilation, vcpkg dependency resolution, Qt 6 integration,
    runtime DLL deployment, and optional automated test suite execution.

.PARAMETER Config
    Build configuration: "Release" (default), "Debug", "RelWithDebInfo", or "MinSizeRel".

.PARAMETER BuildDir
    Build directory path relative to workspace root (default: "build").

.PARAMETER Target
    Optional specific CMake target to build (e.g. "SightlineAppQt", "TestSightlineVideo", "PelcoDCore").
    Builds all targets if empty.

.PARAMETER Clean
    If specified, removes the build directory before configuring.

.PARAMETER Reconfigure
    Force re-running CMake configuration even if CMakeCache.txt already exists.

.PARAMETER RunTests
    If specified, executes CTest after a successful build.

.PARAMETER QtDir
    Path to Qt 6 MSVC directory. Defaults to C:\Qt\6.6.1\msvc2019_64 or auto-detected in C:\Qt.

.PARAMETER VcpkgToolchain
    Path to vcpkg CMake toolchain file. Defaults to C:\vcpkg\scripts\buildsystems\vcpkg.cmake.

.PARAMETER ParallelJobs
    Number of parallel compilation processes. Defaults to logical CPU count.

.EXAMPLE
    .\scripts\Build.ps1
    Builds all targets in Release configuration.

.EXAMPLE
    .\scripts\Build.ps1 -Config Debug -RunTests
    Builds all targets in Debug configuration and runs all unit tests.

.EXAMPLE
    .\scripts\Build.ps1 -Target SightlineAppQt
    Builds only the Sightline SLA Qt control application.

.EXAMPLE
    .\scripts\Build.ps1 -Clean -Config Release
    Performs a clean build from scratch.
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",

    [string]$BuildDir = "build",
    [string]$Target = "",
    [switch]$Clean,
    [switch]$Reconfigure,
    [switch]$RunTests,
    [string]$QtDir = "",
    [string]$VcpkgToolchain = "",
    [int]$ParallelJobs = 0
)

$ErrorActionPreference = "Stop"

$startTime = [System.Diagnostics.Stopwatch]::StartNew()
$workspaceRoot = (Get-Item -Path "$PSScriptRoot\..").FullName
$buildFullPath = [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $BuildDir))

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host " Pelco-D Controller - Windows Build & Pipeline Automator        " -ForegroundColor Cyan
Write-Host " Configuration : $Config" -ForegroundColor Gray
Write-Host " Workspace     : $workspaceRoot" -ForegroundColor Gray
Write-Host " Build Dir     : $buildFullPath" -ForegroundColor Gray
if ($Target) {
    Write-Host " Target        : $Target" -ForegroundColor Gray
}
Write-Host "=================================================================" -ForegroundColor Cyan

# 1. Detect CMake & CTest
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake is not found in PATH! Please install CMake 3.16+ from https://cmake.org/download/"
    exit 1
}

# 2. Detect vcpkg CMake Toolchain
if (-not $VcpkgToolchain) {
    $vcpkgCandidates = @(
        "C:\vcpkg\scripts\buildsystems\vcpkg.cmake",
        "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
    )
    foreach ($cand in $vcpkgCandidates) {
        if ($cand -and (Test-Path $cand)) {
            $VcpkgToolchain = $cand
            break
        }
    }
}

if (-not $VcpkgToolchain -or -not (Test-Path $VcpkgToolchain)) {
    Write-Error "vcpkg.cmake toolchain not found! Specify -VcpkgToolchain or set VCPKG_ROOT."
    exit 1
}
Write-Host "[OK] Found vcpkg toolchain : $VcpkgToolchain" -ForegroundColor Green

# 3. Detect Qt 6
if (-not $QtDir) {
    $qtCandidates = @(
        $env:QTDIR,
        "C:\Qt\6.6.1\msvc2019_64",
        "C:\Qt\6.6.2\msvc2019_64",
        "C:\Qt\6.5.3\msvc2019_64"
    )
    foreach ($cand in $qtCandidates) {
        if ($cand -and (Test-Path "$cand\bin\qmake.exe")) {
            $QtDir = $cand
            break
        }
    }

    if (-not $QtDir -and (Test-Path "C:\Qt")) {
        $found = Get-ChildItem -Path "C:\Qt" -Directory -Recurse -Depth 2 -Filter "msvc*" -ErrorAction SilentlyContinue |
                 Where-Object { Test-Path "$($_.FullName)\bin\qmake.exe" } |
                 Select-Object -First 1
        if ($found) {
            $QtDir = $found.FullName
        }
    }
}

if (-not $QtDir -or -not (Test-Path "$QtDir\bin\qmake.exe")) {
    Write-Error "Qt 6 MSVC directory not found! Specify -QtDir or set QTDIR environment variable."
    exit 1
}
Write-Host "[OK] Found Qt 6 prefix     : $QtDir" -ForegroundColor Green

# Set parallel CPU count
if ($ParallelJobs -le 0) {
    $ParallelJobs = [Environment]::ProcessorCount
}
Write-Host "[OK] Parallel build jobs   : $ParallelJobs" -ForegroundColor Green

# 4. Clean Build Directory (if requested)
if ($Clean) {
    Write-Host "`n[1/4] Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $buildFullPath) {
        Remove-Item -Path $buildFullPath -Recurse -Force
        Write-Host "[OK] Build directory removed." -ForegroundColor Green
    }
}

# 5. CMake Configure
$cacheFile = Join-Path $buildFullPath "CMakeCache.txt"
$needsConfigure = $Clean -or $Reconfigure -or (-not (Test-Path $cacheFile))

# Validate Visual Studio instance health in existing cache to prevent stale version lock
if ((-not $needsConfigure) -and (Test-Path $cacheFile)) {
    $cacheContent = Get-Content -Path $cacheFile -Raw
    # If CMAKE_GENERATOR_INSTANCE has a pinned version suffix (e.g. ,version=17.14.40.0) that no longer matches
    if ($cacheContent -match 'CMAKE_GENERATOR_INSTANCE:[^=]*=([^,\r\n]+),version=[^\r\n]+') {
        $vsPath = $Matches[1]
        Write-Host "[INFO] Detected pinned Visual Studio version in CMakeCache.txt. Normalizing..." -ForegroundColor Yellow
        $cleanedContent = $cacheContent -replace '(?m)^(CMAKE_GENERATOR_INSTANCE:[^=]*=[^,\r\n]+),version=[^\r\n]+', '$1'
        Set-Content -Path $cacheFile -Value $cleanedContent -NoNewline
        if (-not (Test-Path $vsPath)) {
            Write-Host "[WARN] Visual Studio path in cache does not exist ($vsPath). Forcing reconfigure..." -ForegroundColor Yellow
            $needsConfigure = $true
        }
    }
}

if ($needsConfigure) {
    Write-Host "`n[2/4] Configuring project with CMake..." -ForegroundColor Yellow

    $cmakeArgs = @(
        "-B", $buildFullPath,
        "-S", $workspaceRoot,
        "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain",
        "-DCMAKE_PREFIX_PATH=$QtDir"
    )

    Write-Host "Running: cmake $($cmakeArgs -join ' ')" -ForegroundColor DarkGray
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
    Write-Host "[OK] CMake configuration succeeded." -ForegroundColor Green
} else {
    Write-Host "`n[2/4] CMake build tree is already configured (skipping configure step)." -ForegroundColor DarkGray
}

# 6. CMake Build
Write-Host "`n[3/4] Compiling targets ($Config)..." -ForegroundColor Yellow

$buildArgs = @(
    "--build", $buildFullPath,
    "--config", $Config,
    "--parallel", $ParallelJobs
)

if ($Target) {
    $buildArgs += @("--target", $Target)
}

Write-Host "Running: cmake $($buildArgs -join ' ')" -ForegroundColor DarkGray
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
Write-Host "[OK] Compilation completed successfully." -ForegroundColor Green

# 7. Deploy Runtime Dependencies (vcpkg DLLs & windeployqt)
Write-Host "`n[4/4] Verifying and deploying runtime DLLs..." -ForegroundColor Yellow

$vcpkgBinDir = if ($Config -eq "Debug") {
    Join-Path $workspaceRoot "vcpkg_installed\x64-windows\debug\bin"
} else {
    Join-Path $workspaceRoot "vcpkg_installed\x64-windows\bin"
}

# Ensure essential vcpkg runtime DLLs (FFmpeg, glog) are co-located in app output directories
$appOutputDirs = @(
    (Join-Path $buildFullPath "app-sightline-qt\$Config"),
    (Join-Path $buildFullPath "app-video-qt\$Config"),
    (Join-Path $buildFullPath "app-pelcod-qt\$Config")
)

if (Test-Path $vcpkgBinDir) {
    foreach ($outDir in $appOutputDirs) {
        if (Test-Path $outDir) {
            $existingExe = Get-ChildItem -Path $outDir -Filter "*.exe" -ErrorAction SilentlyContinue
            if ($existingExe) {
                # Copy essential multimedia and utility DLLs
                $dllFilters = @("avformat*.dll", "avcodec*.dll", "avutil*.dll", "swscale*.dll", "swresample*.dll", "glog*.dll", "gflags*.dll")
                foreach ($filt in $dllFilters) {
                    Get-ChildItem -Path $vcpkgBinDir -Filter $filt -ErrorAction SilentlyContinue | ForEach-Object {
                        $dest = Join-Path $outDir $_.Name
                        if (-not (Test-Path $dest)) {
                            Copy-Item -Path $_.FullName -Destination $dest -Force
                        }
                    }
                }
            }
        }
    }
}
Write-Host "[OK] Runtime DLL deployment verified." -ForegroundColor Green

# 8. Run Automated Test Suite (if requested)
if ($RunTests) {
    Write-Host "`n[TEST] Executing automated test suite..." -ForegroundColor Cyan

    $env:PATH = "$QtDir\bin;$vcpkgBinDir;" + $env:PATH
    $env:QT_QPA_PLATFORM = "offscreen"

    $ctestArgs = @(
        "--test-dir", $buildFullPath,
        "-C", $Config,
        "--output-on-failure",
        "--parallel", $ParallelJobs
    )

    Write-Host "Running: ctest $($ctestArgs -join ' ')" -ForegroundColor DarkGray
    & ctest @ctestArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "One or more tests failed! Exit code: $LASTEXITCODE"
        exit $LASTEXITCODE
    }
    Write-Host "[OK] All test suites passed successfully!" -ForegroundColor Green
}

$startTime.Stop()
$elapsedSeconds = [Math]::Round($startTime.Elapsed.TotalSeconds, 1)

Write-Host "`n=================================================================" -ForegroundColor Cyan
Write-Host " BUILD SUCCEEDED in ${elapsedSeconds}s" -ForegroundColor Green
Write-Host "=================================================================" -ForegroundColor Cyan
