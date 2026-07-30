# Builds BirriMonitor.sln (Debug|x64 and Release|x64).
# Place this file in the project root, next to BirriMonitor.sln.
# Automatically builds MinHook first if it hasn't been built yet.
# Run from a Visual Studio Developer PowerShell.
$ErrorActionPreference = "Stop"

$scriptDir   = $PSScriptRoot
$solution    = Join-Path $scriptDir "BirriMonitor.sln"
$minhookBuild = Join-Path $scriptDir "External\minhook\build_cmake"

if (-not (Get-Command msbuild -ErrorAction SilentlyContinue)) {
    Write-Host "[!] MSBuild not found in PATH. Run this from a Developer PowerShell." -ForegroundColor Red
    exit 1
}

$debugLib   = Join-Path $minhookBuild "Debug\minhook.x64d.lib"
$releaseLib = Join-Path $minhookBuild "Release\minhook.x64.lib"

if (-not (Test-Path $debugLib) -or -not (Test-Path $releaseLib)) {
    Write-Host "MinHook not built yet, building it first..." -ForegroundColor Yellow
    & (Join-Path $scriptDir "External\build_minhook.ps1")
    if ($LASTEXITCODE -ne 0) { Write-Host "MinHook build failed, aborting." -ForegroundColor Red; exit 1 }
} else {
    Write-Host "MinHook already built, skipping." -ForegroundColor Green
}

Write-Host "Building BirriMonitor.sln - Debug|x64..." -ForegroundColor Yellow
& msbuild $solution /p:Configuration=Debug /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) { Write-Host "Debug build failed!" -ForegroundColor Red; exit 1 }

Write-Host "Building BirriMonitor.sln - Release|x64..." -ForegroundColor Yellow
& msbuild $solution /p:Configuration=Release /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) { Write-Host "Release build failed!" -ForegroundColor Red; exit 1 }

Write-Host "Build complete! Output in $scriptDir\x64\Debug\ and $scriptDir\x64\Release\" -ForegroundColor Green