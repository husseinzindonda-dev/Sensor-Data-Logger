# build.ps1 - Build script for Sensor Data Logger
# Run with: .\build.ps1
# Run tests only: .\build.ps1 -TestOnly
# Skip running after build: .\build.ps1 -NoRun

param(
    [switch]$TestOnly,
    [switch]$NoRun
)

$CFLAGS = "-Wall -Wextra -Werror -std=c11 -g"
$CORE = "src/buffer.c src/sensors.c src/sensor_manager.c src/logger.c"

Write-Host "=== Sensor Data Logger Build Script ===" -ForegroundColor Cyan
Write-Host "Project: $PSScriptRoot" -ForegroundColor Gray

# =============================================================================
# 1. Clean
# =============================================================================
Write-Host "`n[1/4] Cleaning previous build..." -ForegroundColor Yellow
if (Test-Path "build") {
    Remove-Item -Path "build" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "   Removed old 'build' directory" -ForegroundColor Green
}
else {
    Write-Host "   No previous build found (that's OK)" -ForegroundColor Gray
}

# =============================================================================
# 2. Create build directory
# =============================================================================
Write-Host "`n[2/4] Creating build directory..." -ForegroundColor Yellow
try {
    New-Item -ItemType Directory -Path "build" -Force | Out-Null
    Write-Host "   Created 'build' directory" -ForegroundColor Green
}
catch {
    Write-Host "   ERROR: Could not create build directory" -ForegroundColor Red
    exit 1
}

# =============================================================================
# 3. Build
# =============================================================================
Write-Host "`n[3/4] Compiling..." -ForegroundColor Yellow

$allOk = $true

function RunCompile {
    param([string]$Label, [string]$Command)
    Write-Host "`n   $Label" -ForegroundColor Gray
    Write-Host "   > $Command" -ForegroundColor DarkGray
    Write-Host "   Compiling..." -NoNewline
    $result = Invoke-Expression "$Command 2>&1"
    if ($LASTEXITCODE -eq 0) {
        Write-Host " OK" -ForegroundColor Green
    }
    else {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host $result -ForegroundColor Red
    }
    return $LASTEXITCODE
}

if (-not $TestOnly) {
    $exitCode = RunCompile "Main app       -> build/sensor_logger.exe" "gcc $CORE src/main.c -o build/sensor_logger.exe $CFLAGS"
    if ($exitCode -ne 0) { $allOk = $false }
}

$exitCode = RunCompile "Tests (buffer) -> build/test_buffer.exe" "gcc src/buffer.c tests/test_buffer.c -o build/test_buffer.exe $CFLAGS"
if ($exitCode -ne 0) { $allOk = $false }

$exitCode = RunCompile "Tests (sensor) -> build/test_sensor.exe" "gcc src/buffer.c src/sensors.c tests/test_sensor.c -o build/test_sensor.exe $CFLAGS -lm"
if ($exitCode -ne 0) { $allOk = $false }

$exitCode = RunCompile "Tests (manager)-> build/test_manager.exe" "gcc src/buffer.c src/sensors.c src/sensor_manager.c tests/test_manager.c -o build/test_manager.exe $CFLAGS -lm"
if ($exitCode -ne 0) { $allOk = $false }

if (-not $allOk) {
    Write-Host "`nBuild failed - see errors above." -ForegroundColor Red
    exit 1
}

Write-Host "`n   All targets compiled successfully." -ForegroundColor Green

if ($NoRun) {
    Write-Host "`n=== Build complete ===" -ForegroundColor Cyan
    exit 0
}

# =============================================================================
# 4. Run
# =============================================================================
Write-Host "`n[4/4] Running..." -ForegroundColor Yellow

$anyFailed = $false

function RunExe {
    param([string]$Label, [string]$Path)
    Write-Host ("`n" + ("=" * 60)) -ForegroundColor DarkCyan
    Write-Host "  $Label" -ForegroundColor White
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    & $Path
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  Exited: $LASTEXITCODE (OK)" -ForegroundColor Green
    }
    else {
        Write-Host "  Exited: $LASTEXITCODE (FAILED)" -ForegroundColor Red
    }
}

RunExe "Ring Buffer Test Suite"    ".\build\test_buffer.exe"
if ($LASTEXITCODE -ne 0) { $anyFailed = $true }

RunExe "Sensor Layer Test Suite"   ".\build\test_sensor.exe"
if ($LASTEXITCODE -ne 0) { $anyFailed = $true }

RunExe "Sensor Manager Test Suite" ".\build\test_manager.exe"
if ($LASTEXITCODE -ne 0) { $anyFailed = $true }

if (-not $TestOnly) {
    RunExe "Predictive Maintenance Monitor" ".\build\sensor_logger.exe"
    if ($LASTEXITCODE -ne 0) { $anyFailed = $true }
}

# =============================================================================
# Summary
# =============================================================================
Write-Host ("`n" + ("=" * 60)) -ForegroundColor Cyan
if ($anyFailed) {
    Write-Host "  BUILD OK - but one or more runs FAILED" -ForegroundColor Yellow
}
else {
    Write-Host "  ALL DONE - build and runs passed" -ForegroundColor Green
}
Write-Host ("=" * 60) -ForegroundColor Cyan

Write-Host "`nTo view the dashboard:" -ForegroundColor White
Write-Host "  pip install matplotlib pandas" -ForegroundColor Gray
Write-Host "  python dashboard/dashboard.py" -ForegroundColor Gray
