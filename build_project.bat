@echo off
REM Builds BirriMonitor.sln (Debug|x64 and Release|x64).
REM Place this file in the project root, next to BirriMonitor.sln.
REM Automatically builds MinHook first if it hasn't been built yet.
REM Run from a Visual Studio Developer Command Prompt.

set SCRIPT_DIR=%~dp0
set SOLUTION=%SCRIPT_DIR%BirriMonitor.sln
set MINHOOK_BUILD=%SCRIPT_DIR%External\minhook\build_cmake

where msbuild >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] MSBuild not found in PATH. Run this from a Developer Command Prompt.
    exit /b 1
)

if not exist "%MINHOOK_BUILD%\Debug\minhook.x64d.lib" goto :build_minhook
if not exist "%MINHOOK_BUILD%\Release\minhook.x64.lib" goto :build_minhook
echo [*] MinHook already built, skipping.
goto :build_main

:build_minhook
echo [*] MinHook not built yet, building it first...
call "%SCRIPT_DIR%External\build_minhook.bat"
if %ERRORLEVEL% NEQ 0 (
    echo [!] MinHook build failed, aborting.
    exit /b 1
)

:build_main
echo [*] Building BirriMonitor.sln - Debug^|x64...
msbuild "%SOLUTION%" /p:Configuration=Debug /p:Platform=x64 /m
if %ERRORLEVEL% NEQ 0 (
    echo [!] Debug build failed
    exit /b 1
)

echo [*] Building BirriMonitor.sln - Release^|x64...
msbuild "%SOLUTION%" /p:Configuration=Release /p:Platform=x64 /m
if %ERRORLEVEL% NEQ 0 (
    echo [!] Release build failed
    exit /b 1
)

echo.
echo [+] Build complete! Output in %SCRIPT_DIR%x64\Debug\ and %SCRIPT_DIR%x64\Release\