param(
    [switch]$RestartOverlay
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectDir

# Locate compiler (portable w64devkit or system PATH g++)
$GppPath = "$env:TEMP\toolchain\w64devkit\bin\g++.exe"
if (-not (Test-Path $GppPath)) {
    $cmd = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        $GppPath = $cmd.Source
    } else {
        Write-Error "g++.exe not found in $env:TEMP\toolchain\w64devkit\bin or system PATH."
        exit 1
    }
}

Write-Host "[1/3] Compiling unit & integration test suite (spotter_tests.exe)..." -ForegroundColor Cyan
& $GppPath -O2 -std=c++17 -static "$ProjectDir\spotter_tests.cpp" -o "$ProjectDir\spotter_tests.exe" -luser32 -lgdi32 -lwinmm
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to compile spotter_tests.cpp!"
    exit $LASTEXITCODE
}

Write-Host "[2/3] Running unit & integration test suite..." -ForegroundColor Cyan
& "$ProjectDir\spotter_tests.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Test suite failed! Aborting spotter_overlay.exe build to prevent regressions."
    exit $LASTEXITCODE
}

Write-Host "[3/3] Compiling production overlay (spotter_overlay.exe)..." -ForegroundColor Cyan
$wasRunning = $null -ne (Get-Process -Name "spotter_overlay" -ErrorAction SilentlyContinue)
if ($wasRunning) {
    Stop-Process -Name "spotter_overlay" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 250
}

& $GppPath -O2 -std=c++17 -mwindows -static "$ProjectDir\spotter_overlay.cpp" -o "$ProjectDir\spotter_overlay.exe" -luser32 -lgdi32 -lwinmm
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to compile spotter_overlay.cpp!"
    exit $LASTEXITCODE
}

Write-Host "[DONE] Build and all tests succeeded!" -ForegroundColor Green

if ($RestartOverlay -or $wasRunning) {
    Write-Host "Starting spotter_overlay.exe..." -ForegroundColor Green
    Start-Process -FilePath "$ProjectDir\spotter_overlay.exe" -WorkingDirectory $ProjectDir
}
