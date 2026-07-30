# Build MinHook using PowerShell
$ErrorActionPreference = "Stop"

$minhookDir = "C:\Users\qhuyy\source\repos\BirriMonitor\External\minhook"
$solutionPath = "$minhookDir\build\VC16\MinHookVC16.sln"

Write-Host "Building MinHook..." -ForegroundColor Green

# Build Debug x64
Write-Host "Building Debug|x64..." -ForegroundColor Yellow
& msbuild $solutionPath /p:Configuration=Debug /p:Platform=x64 /m /nologo

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# Build Release x64
Write-Host "Building Release|x64..." -ForegroundColor Yellow
& msbuild $solutionPath /p:Configuration=Release /p:Platform=x64 /m /nologo

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "MinHook build complete!" -ForegroundColor Green
Write-Host "Output: $minhookDir\build\VC16\x64\" -ForegroundColor Cyan
