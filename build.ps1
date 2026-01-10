# build.ps1 - Simple build script for Windows
# Run with: .\build.ps1  OR  powershell -ExecutionPolicy Bypass -File build.ps1

Write-Host "=== Sensor Data Logger Build Script ===" -ForegroundColor Cyan
Write-Host "Project: $PSScriptRoot" -ForegroundColor Gray

# 1. Clean previous build
Write-Host "`n[1/3] Cleaning previous build..." -ForegroundColor Yellow
if (Test-Path "build") {
    Remove-Item -Path "build" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "   Removed old 'build' directory" -ForegroundColor Green
} else {
    Write-Host "   No previous build found (that's OK)" -ForegroundColor Gray
}

# 2. Create fresh build directory
Write-Host "`n[2/3] Creating build directory..." -ForegroundColor Yellow
try {
    New-Item -ItemType Directory -Path "build" -Force | Out-Null
    Write-Host "   Created 'build' directory" -ForegroundColor Green
} catch {
    Write-Host "   ERROR: Could not create build directory" -ForegroundColor Red
    exit 1
}

# 3. Compile all C files
Write-Host "`n[3/3] Compiling source files..." -ForegroundColor Yellow

# List all source files - add more here as you create them
$sourceFiles = @(
    "src/main.c",
    "src/buffer.c"
)

# Show what we're compiling
Write-Host "   Compiling:" -ForegroundColor Gray
foreach ($file in $sourceFiles) {
    Write-Host "     - $file" -ForegroundColor Gray
}

# Build the compile command
$compileCommand = "gcc $sourceFiles -o build/sensor_logger.exe -Wall -Wextra -Werror -std=c11 -g"

# Show the command (for debugging)
Write-Host "`n   Command: $compileCommand" -ForegroundColor DarkGray

# Execute compilation
Write-Host "   Compiling..." -NoNewline
$compileResult = Invoke-Expression $compileCommand 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host " SUCCESS" -ForegroundColor Green
    
    # 4. Run the program
    Write-Host "`n" + ("=" * 60) -ForegroundColor DarkCyan
    Write-Host "RUNNING PROGRAM" -ForegroundColor White -BackgroundColor DarkBlue
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    
    # Capture and display program output
    $output = & ".\build\sensor_logger.exe" 2>&1
    $output
    
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    Write-Host "Program exited with code: $LASTEXITCODE" -ForegroundColor Gray
    
    # Check if program crashed
    if ($LASTEXITCODE -ne 0) {
        Write-Host "WARNING: Program returned non-zero exit code" -ForegroundColor Yellow
    }
    
} else {
    Write-Host " FAILED" -ForegroundColor Red
    Write-Host "`nCompilation errors:" -ForegroundColor Red
    Write-Host $compileResult -ForegroundColor Red
    Write-Host "`nExit code: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`n=== Build process complete ===" -ForegroundColor Cyan