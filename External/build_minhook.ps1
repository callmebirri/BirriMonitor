# Builds MinHook (Debug|x64 and Release|x64) via CMake.
# Place this file inside External\ (next to the minhook\ folder).
# Run from a Visual Studio Developer PowerShell.
$ErrorActionPreference = "Stop"

$scriptDir  = $PSScriptRoot
$minhookDir = Join-Path $scriptDir "minhook"
$buildDir   = Join-Path $minhookDir "build_cmake"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "[!] cmake not found in PATH. Run this from a Developer PowerShell." -ForegroundColor Red
    exit 1
}

if (Test-Path (Join-Path $buildDir "CMakeCache.txt")) {
    Write-Host "Removing stale CMake cache from a previous run..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "Configuring MinHook (CMake, auto-detected VS generator, v145 toolset)..." -ForegroundColor Green
& cmake -S $minhookDir -B $buildDir -A x64 -T v145
if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed!" -ForegroundColor Red; exit 1 }

Write-Host "Building Debug|x64..." -ForegroundColor Yellow
& cmake --build $buildDir --config Debug
if ($LASTEXITCODE -ne 0) { Write-Host "Debug build failed!" -ForegroundColor Red; exit 1 }

Write-Host "Building Release|x64..." -ForegroundColor Yellow
& cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { Write-Host "Release build failed!" -ForegroundColor Red; exit 1 }

Write-Host "MinHook build complete!" -ForegroundColor Green
Write-Host "  Debug lib:   $buildDir\Debug\minhook.x64d.lib" -ForegroundColor Cyan
Write-Host "  Release lib: $buildDir\Release\minhook.x64.lib" -ForegroundColor Cyan