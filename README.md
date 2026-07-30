# BirriMonitor

A Windows network traffic monitoring toolkit that intercepts HTTP/HTTPS requests using API hooking techniques. The project consists of an injection DLL that hooks WinHTTP functions, a launcher to inject the DLL into target processes, and a logger that displays captured network data in real-time via IPC (Inter-Process Communication).

## Project Structure

```
BirriMonitor/
├── BirriMonitor.sln              # Solution file
├── BirriMonitor.vcxproj          # DLL project (WinHTTP hook)
├── dllmain.cpp                   # DLL entry point
├── WinHttpHooks.cpp/.h           # WinHTTP hook implementation
├── Shared/
│   ├── HookEngine.cpp/.h         # Hook engine core
│   ├── IpcClient.cpp/.h          # IPC client (DLL side)
│   └── IpcCommon.h               # Shared IPC protocol definitions
├── BirriLauncher/
│   ├── BirriLauncher.vcxproj
│   └── main.cpp                  # DLL injector (3 methods)
├── BirriLogger/
│   ├── BirriLogger.vcxproj
│   └── main.cpp                  # Network traffic logger
├── External/
│   ├── minhook/                  # MinHook library (git submodule)
│   ├── build_minhook.bat/.ps1    # Build script for MinHook
│   └── build_cmake/              # CMake build output (generated)
├── build_project.bat/.ps1        # Main build script
└── x64/
    ├── Debug/                    # Build output (generated)
    └── Release/                  # Build output (generated)
```

## Requirements

- Windows 10 or later (x64)
- Visual Studio 2022 version 17.13+ or Visual Studio 2026 (v145 toolset)
- CMake 3.15 or later
- Git (to clone MinHook submodule)

## Build Environment

All build scripts must be run from a **Developer Command Prompt for VS** or **Developer PowerShell for VS**. Do not use a regular terminal as MSBuild and Visual Studio environment variables are required.

To open Developer Command Prompt:
- From Start Menu: search for "Developer Command Prompt for VS"
- From Visual Studio: `Tools > Command Line > Developer Command Prompt`
- Or manually run: `"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"` (adjust path based on VS edition)

## Build Instructions

### Step 1: Prepare MinHook

If the `External/minhook` directory is empty, clone MinHook:

```bat
git clone https://github.com/TsudaKageyu/minhook.git External/minhook
```

Or if the project uses git submodules:

```bat
git submodule update --init --recursive
```

### Step 2: Build the entire project

Run one of the following scripts from the Developer Command Prompt (in the project root directory):

**Using Batch:**
```bat
build_project.bat
```

**Using PowerShell:**
```powershell
.\build_project.ps1
```

The script will automatically:
1. Check if MinHook has been built, and build it first if not
2. Build `BirriMonitor.sln` with Debug|x64 and Release|x64 configurations
3. Output files to `x64/Debug/` and `x64/Release/`

### Build MinHook separately

If you only want to build MinHook:

```bat
External\build_minhook.bat
```
```powershell
.\External\build_minhook.ps1
```

### Build from command line (without scripts)

```bat
msbuild BirriMonitor.sln /p:Configuration=Debug /p:Platform=x64 /m
msbuild BirriMonitor.sln /p:Configuration=Release /p:Platform=x64 /m
```

## Usage

### Overview

1. Start `BirriLogger` first to listen for connections from the DLL
2. Inject `BirriMonitor.dll` into the target process using `BirriLauncher`
3. `BirriLogger` will display intercepted HTTP requests and responses in real-time

### Step 1: Start the Logger

Open a new terminal and run:

```bat
x64\Debug\BirriLogger.exe
```
or
```bat
x64\Release\BirriLogger.exe
```

The logger will display: `Waiting for DLL connection...`

### Step 2: Inject DLL into target process

Use `BirriLauncher` with one of three methods:

#### Method A: Spawn suspended process + inject

Create a new process in suspended state, inject the DLL, then resume:

```bat
BirriLauncher.exe -a "C:\Path\To\Target.exe" "C:\Path\To\BirriMonitor.dll"
```

Example:
```bat
x64\Debug\BirriLauncher.exe -a "C:\Windows\System32\curl.exe" "C:\path\to\BirriMonitor\x64\Debug\BirriMonitor.dll"
```

#### Method B: Inject into running process

Inject DLL into an already running process by PID:

```bat
BirriLauncher.exe -b <PID> "C:\Path\To\BirriMonitor.dll"
```

Example:
```bat
x64\Debug\BirriLauncher.exe -b 12345 "C:\path\to\BirriMonitor\x64\Debug\BirriMonitor.dll"
```

Get the PID using Task Manager or the command:
```bat
tasklist | findstr "process_name"
```

#### Method C: DLL hijacking (not implemented)

```bat
BirriLauncher.exe -c <target_directory> <dll_name>
```

## Technical Details

### Hook Engine

Uses the [MinHook](https://github.com/TsudaKageyu/minhook) library to hook 5 WinHTTP functions:
- `WinHttpOpenRequest`
- `WinHttpSendRequest`
- `WinHttpReceiveResponse`
- `WinHttpReadData`
- `WinHttpQueryHeaders`

### IPC Protocol

Communication between DLL and Logger uses Windows Named Pipes with the following protocol:
- Magic number: `0xB17A`
- Pipe name: `\\.\pipe\BirriMonitorPipe` (defined in `IpcCommon.h`)
- Message types: `HOOKS_READY`, `HTTP_REQUEST`, `HTTP_RESPONSE`, `LOG_MESSAGE`
- Buffer size: 64KB (`PIPE_BUFFER_SIZE`)

### Re-entrancy Protection

Uses TLS (Thread Local Storage) to prevent recursive hook calls. When inside a hook, subsequent WinHTTP calls from the DLL bypass the hook and call the original function directly.

## Known Limitations

- x64 only (WinHTTP 64-bit)
- Hooks WinHTTP only; does not support WinINet, socket API, or other HTTP libraries
- Method C (DLL hijacking) is not implemented
- DLL must be injected before the target process calls WinHTTP functions to ensure complete hooking

## License

This project is licensed under the **PolyForm Noncommercial License 1.0.0**.

You are free to use, modify, and distribute this software for **non-commercial purposes** only. Commercial use is strictly prohibited without explicit permission from the author.

See the [LICENSE](LICENSE) file for the full license text.

## Author & Credits

**Author:** birri (callmebirri)  
**GitHub:** [github.com/callmebirri](https://github.com/callmebirri)

**Built with:**
- [MinHook](https://github.com/TsudaKageyu/minhook) - The minimalistic x86/x64 API hooking library for Windows.

## Contributing

Contributions, issues, and feature requests are welcome!  
Please read the [CODE_OF_CONDUCT](CODE_OF_CONDUCT.md) first.  
Feel free to check the [issues page](https://github.com/callmebirri/BirriMonitor/issues).

## Disclaimer

**This software is provided "as is", without any warranty.** Use at your own risk. The author is not responsible for any damage or data loss caused by this tool.

---
<div align="center">

Developed with ❤️ by **birri**

Copyright © 2026 birri

</div>
