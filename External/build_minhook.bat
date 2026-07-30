@echo off
REM Builds MinHook (Debug|x64 and Release|x64) via CMake.
REM Place this file inside External\ (next to the minhook\ folder).
REM Run from a Visual Studio Developer Command Prompt.

where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] cmake not found in PATH. Run this from a Developer Command Prompt.
    exit /b 1
)

set SCRIPT_DIR=%~dp0
set MINHOOK_DIR=%SCRIPT_DIR%minhook
set BUILD_DIR=%MINHOOK_DIR%\build_cmake

if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [*] Removing stale CMake cache from a previous run...
    rmdir /s /q "%BUILD_DIR%"
)

echo [*] Configuring MinHook (CMake, auto-detected VS generator, v145 toolset)...
cmake -S "%MINHOOK_DIR%" -B "%BUILD_DIR%" -A x64 -T v145
if %ERRORLEVEL% NEQ 0 (
    echo [!] CMake configure failed
    exit /b 1
)

echo [*] Building Debug^|x64...
cmake --build "%BUILD_DIR%" --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo [!] Debug build failed
    exit /b 1
)

echo [*] Building Release^|x64...
cmake --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [!] Release build failed
    exit /b 1
)

echo.
echo [+] MinHook build complete!
echo     Debug lib:   %BUILD_DIR%\Debug\minhook.x64d.lib
echo     Release lib: %BUILD_DIR%\Release\minhook.x64.lib