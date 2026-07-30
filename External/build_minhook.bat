@echo off
echo Building MinHook library...
echo.

REM Check if VS Developer Command Prompt is available
where msbuild >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] MSBuild not found in PATH
    echo Please run this script from Visual Studio Developer Command Prompt
    echo Or run: "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\Tools\Launch-VsDevShell.ps1"
    exit /b 1
)

set MINHOOK_DIR=C:\Users\qhuyy\source\repos\BirriMonitor\External\minhook
set SOLUTION=%MINHOOK_DIR%\build\VC16\MinHookVC16.sln

echo [*] Building Debug|x64...
msbuild "%SOLUTION%" /p:Configuration=Debug /p:Platform=x64 /m

echo [*] Building Release|x64...
msbuild "%SOLUTION%" /p:Configuration=Release /p:Platform=x64 /m

echo.
echo [+] MinHook build complete!
echo Output should be in: %MINHOOK_DIR%\build\VC16\x64\
pause
