param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "build/coverage_report",
    [string]$Pattern = "*",
    [int]$ParallelJobs = 0,
    [double]$Threshold = 90.0,
    [switch]$FailUnderThreshold
)

$ErrorActionPreference = "Stop"

# Auto-relaunch with pwsh if running on legacy Windows PowerShell (5.1) for maximum parallel performance
if ($PSVersionTable.PSVersion.Major -lt 7 -and (Get-Command pwsh -ErrorAction SilentlyContinue)) {
    Write-Host "Elevating to PowerShell 7 (pwsh) for high-performance parallel instrumentation..." -ForegroundColor Cyan
    & pwsh -ExecutionPolicy Bypass -File $PSCommandPath @args
    exit $LASTEXITCODE
}

if ($ParallelJobs -le 0) {
    $cpuCount = [Environment]::ProcessorCount
    $ParallelJobs = [Math]::Max(1, [Math]::Min(8, $cpuCount))
}

# 1. Environment Setup
$workspaceRoot = (Get-Item -Path $PSScriptRoot\..).FullName
$env:PATH = "C:\Program Files\OpenCppCoverage;C:\Qt\6.6.1\msvc2019_64\bin;C:\vcpkg\installed\x64-windows\debug\bin;C:\vcpkg\installed\x64-windows\bin;" + $env:PATH
$env:QT_PLUGIN_PATH = "C:\Qt\6.6.1\msvc2019_64\plugins"
$env:QT_QPA_PLATFORM = "offscreen"

# 2. Locate OpenCppCoverage
$occCandidates = @(
    "OpenCppCoverage.exe",
    "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe",
    "C:\Program Files (x86)\OpenCppCoverage\OpenCppCoverage.exe"
)
$occExe = $null
foreach ($cand in $occCandidates) {
    if (Get-Command $cand -ErrorAction SilentlyContinue) {
        $occExe = (Get-Command $cand).Source
        break
    }
    if (Test-Path $cand) {
        $occExe = $cand
        break
    }
}

if (-not $occExe) {
    Write-Error "OpenCppCoverage.exe not found! Please install it from https://github.com/OpenCppCoverage/OpenCppCoverage/releases"
    exit 1
}

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host " Pelco-D Controller - Automated Code Coverage Pipeline (Phase 4)" -ForegroundColor Cyan
Write-Host " OpenCppCoverage : $occExe" -ForegroundColor Gray
Write-Host " Parallel Jobs   : $ParallelJobs threads" -ForegroundColor Gray
Write-Host " Filter Pattern  : $Pattern" -ForegroundColor Gray
Write-Host "=================================================================" -ForegroundColor Cyan

# 3. Locate All Test Executables
$searchPaths = @(
    "$workspaceRoot\$BuildDir\libs\*\tests\Debug\Test$Pattern.exe",
    "$workspaceRoot\$BuildDir\tests\Debug\Test$Pattern.exe"
)

$testExes = @()
foreach ($sp in $searchPaths) {
    $found = Get-ChildItem -Path $sp -ErrorAction SilentlyContinue
    if ($found) {
        $testExes += $found
    }
}

# Remove duplicates in case of overlapping paths
$testExes = $testExes | Sort-Object -Property FullName -Unique

if ($testExes.Count -eq 0) {
    Write-Error "No test executables found matching '$Pattern' in $BuildDir! Ensure the project is built in Debug mode."
    exit 1
}

Write-Host "Found $($testExes.Count) test executables to instrument.`n" -ForegroundColor Green

# 4. Prepare Coverage Scratch Directory
$dataDir = "$workspaceRoot\$BuildDir\coverage_data"
if (Test-Path $dataDir) {
    Remove-Item -Recurse -Force $dataDir
}
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

# 5. Run Test Binaries Under OpenCppCoverage in Parallel
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$envPath = $env:PATH
$qtPluginPath = $env:QT_PLUGIN_PATH

if ($PSVersionTable.PSVersion.Major -ge 7) {
    # PowerShell 7+ native high-throughput parallel execution
    $completedCount = [System.Threading.Interlocked]::Exchange([ref]0, 0)
    $totalCount = $testExes.Count

    $testExes | ForEach-Object -Parallel {
        $exe = $_
        $occExe = $using:occExe
        $workspaceRoot = $using:workspaceRoot
        $dataDir = $using:dataDir
        $env:PATH = $using:envPath
        $env:QT_PLUGIN_PATH = $using:qtPluginPath
        $env:QT_QPA_PLATFORM = "offscreen"

        $covFile = "$dataDir\$($exe.BaseName).cov"

        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = $occExe
        $pinfo.Arguments = "--modules *`"$($exe.Name)`" --sources `"$workspaceRoot\libs\*`" --sources `"$workspaceRoot\app-*`" --excluded_sources `"$workspaceRoot\libs\*\tests\*`" --excluded_sources `"$workspaceRoot\tests\*`" --excluded_sources `"*vcpkg*`" --excluded_sources `"*googletest*`" --excluded_sources `"*build\*`" --export_type `"binary:$covFile`" --quiet -- `"$($exe.FullName)`""
        $pinfo.UseShellExecute = $false
        $pinfo.CreateNoWindow = $true

        $proc = [System.Diagnostics.Process]::Start($pinfo)
        $finished = $proc.WaitForExit(30000) # 30s safety timeout to prevent hangs
        if (-not $finished) {
            $proc.Kill()
            Write-Warning "  [TIMEOUT] $($exe.Name) exceeded 30s timeout"
        } elseif ($proc.ExitCode -eq 0 -and (Test-Path $covFile)) {
            Write-Host "  [OK] $($exe.Name)" -ForegroundColor Green
        } else {
            Write-Warning "  [FAIL] $($exe.Name) (ExitCode: $($proc.ExitCode))"
        }
    } -ThrottleLimit $ParallelJobs
} else {
    # PowerShell 5.1 Fallback: Sequential execution
    $counter = 1
    foreach ($exe in $testExes) {
        $covFile = "$dataDir\$($exe.BaseName).cov"
        Write-Host "[$counter/$($testExes.Count)] Instrumenting $($exe.Name)..." -ForegroundColor Yellow

        & $occExe `
            --modules "*$($exe.Name)" `
            --sources "$workspaceRoot\libs\*" `
            --sources "$workspaceRoot\app-*" `
            --excluded_sources "*\tests\*" `
            --excluded_sources "*vcpkg*" `
            --excluded_sources "*googletest*" `
            --excluded_sources "*build\*" `
            --export_type "binary:$covFile" `
            --quiet `
            -- $exe.FullName

        $counter++
    }
}

$sw.Stop()
Write-Host ("`nInstrumentation completed in {0:N1}s across $ParallelJobs threads." -f $sw.Elapsed.TotalSeconds) -ForegroundColor Cyan

# Gather generated .cov binary files
$covFiles = Get-ChildItem -Path $dataDir -Filter "*.cov"
$covArgs = @()
foreach ($cf in $covFiles) {
    $covArgs += "--input_coverage"
    $covArgs += $cf.FullName
}

# 6. Merge All Coverage Binaries into Unified Reports
Write-Host "`nMerging $($covArgs.Count / 2) coverage results into unified report..." -ForegroundColor Cyan

$reportDir = "$workspaceRoot\$OutputDir"
if (Test-Path $reportDir) {
    Remove-Item -Recurse -Force $reportDir
}
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$coberturaFile = "$reportDir\coverage.xml"

& $occExe `
    $covArgs `
    --sources "$workspaceRoot\libs\*" `
    --sources "$workspaceRoot\app-*" `
    --excluded_sources "*\tests\*" `
    --excluded_sources "*vcpkg*" `
    --excluded_sources "*googletest*" `
    --excluded_sources "*build\*" `
    --export_type "html:$reportDir" `
    --export_type "cobertura:$coberturaFile"

Write-Host "Coverage report generated: $reportDir\index.html" -ForegroundColor Green
Write-Host "Cobertura XML generated:   $coberturaFile" -ForegroundColor Green

# 7. Parse Cobertura XML and Summarize Coverage Metrics
if (Test-Path $coberturaFile) {
    [xml]$xml = Get-Content $coberturaFile
    $lineRate = [double]$xml.coverage.'line-rate'
    $branchRate = [double]$xml.coverage.'branch-rate'
    $linePct = [Math]::Round($lineRate * 100, 2)
    $branchPct = [Math]::Round($branchRate * 100, 2)

    Write-Host "`n=================================================================" -ForegroundColor Cyan
    Write-Host " Coverage Summary" -ForegroundColor Cyan
    Write-Host "=================================================================" -ForegroundColor Cyan
    Write-Host (" Overall Line Coverage   : {0,6}%" -f $linePct) -ForegroundColor $(if ($linePct -ge $Threshold) { "Green" } else { "Yellow" })
    Write-Host (" Overall Branch Coverage : {0,6}%" -f $branchPct) -ForegroundColor $(if ($branchPct -ge $Threshold) { "Green" } else { "Yellow" })
    Write-Host (" Target Threshold        : {0,6}%" -f $Threshold) -ForegroundColor Gray
    Write-Host "=================================================================`n" -ForegroundColor Cyan

    # Package-level breakdown
    Write-Host "Breakdown by Subsystem:" -ForegroundColor White
    foreach ($pkg in $xml.coverage.packages.package) {
        $pkgLine = [Math]::Round([double]$pkg.'line-rate' * 100, 1)
        $pkgBranch = [Math]::Round([double]$pkg.'branch-rate' * 100, 1)
        $color = if ($pkgLine -ge $Threshold) { "Green" } else { "Yellow" }
        Write-Host (" - {0,-30} Line: {1,5}% | Branch: {2,5}%" -f $pkg.name, $pkgLine, $pkgBranch) -ForegroundColor $color
    }
    Write-Host ""

    if ($FailUnderThreshold -and ($linePct -lt $Threshold)) {
        Write-Error "Code coverage ($linePct%) is below required threshold ($Threshold%)!"
        exit 1
    }
}
