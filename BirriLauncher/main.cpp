#include <windows.h>
#include <iostream>
#include <string>
#include <cstring>

// Method A: Spawn suspended + inject
bool InjectMethodA(const wchar_t* exePath, const wchar_t* dllPath) {
    std::wcout << L"[*] Method A: Spawn suspended + inject" << std::endl;
    
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    // Create process in suspended state
    if (!CreateProcessW(
        exePath,
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr,
        nullptr,
        &si,
        &pi
    )) {
        std::cerr << "[-] CreateProcess failed. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    std::cout << "[+] Process created (PID: " << pi.dwProcessId << "), suspended" << std::endl;
    
    // Allocate memory in target process for DLL path
    SIZE_T dllPathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, nullptr, dllPathSize, 
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        std::cerr << "[-] VirtualAllocEx failed" << std::endl;
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }
    
    // Write DLL path to target process
    if (!WriteProcessMemory(pi.hProcess, remoteMem, dllPath, dllPathSize, nullptr)) {
        std::cerr << "[-] WriteProcessMemory failed" << std::endl;
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }
    
    // Get LoadLibraryW address
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(kernel32, "LoadLibraryW");
    
    // Create remote thread to call LoadLibraryW
    HANDLE remoteThread = CreateRemoteThread(pi.hProcess, nullptr, 0, 
                                            (LPTHREAD_START_ROUTINE)loadLibraryAddr,
                                            remoteMem, 0, nullptr);
    if (!remoteThread) {
        std::cerr << "[-] CreateRemoteThread failed" << std::endl;
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }
    
    std::cout << "[+] Waiting for DLL injection..." << std::endl;
    WaitForSingleObject(remoteThread, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeThread(remoteThread, &exitCode);
    
    CloseHandle(remoteThread);
    VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
    
    if (exitCode == 0) {
        std::cerr << "[-] DLL injection failed (LoadLibrary returned 0)" << std::endl;
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }
    
    std::cout << "[+] DLL injected successfully" << std::endl;
    
    // TODO: Wait for handshake from DLL before resuming
    // For now, just resume
    std::cout << "[*] Waiting for handshake..." << std::endl;
    Sleep(2000);  // Simple delay, should wait for actual handshake
    
    // Resume main thread
    ResumeThread(pi.hThread);
    std::cout << "[+] Process resumed" << std::endl;
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return true;
}

// Method B: Inject into running process
bool InjectMethodB(DWORD pid, const wchar_t* dllPath) {
    std::cout << "[*] Method B: Inject into running process (PID: " << pid << ")" << std::endl;
    
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "[-] OpenProcess failed. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    // Allocate memory for DLL path
    SIZE_T dllPathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, dllPathSize,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        std::cerr << "[-] VirtualAllocEx failed" << std::endl;
        CloseHandle(hProcess);
        return false;
    }
    
    // Write DLL path
    if (!WriteProcessMemory(hProcess, remoteMem, dllPath, dllPathSize, nullptr)) {
        std::cerr << "[-] WriteProcessMemory failed" << std::endl;
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    // Get LoadLibraryW address
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(kernel32, "LoadLibraryW");
    
    // Create remote thread
    HANDLE remoteThread = CreateRemoteThread(hProcess, nullptr, 0,
                                            (LPTHREAD_START_ROUTINE)loadLibraryAddr,
                                            remoteMem, 0, nullptr);
    if (!remoteThread) {
        std::cerr << "[-] CreateRemoteThread failed" << std::endl;
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    WaitForSingleObject(remoteThread, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeThread(remoteThread, &exitCode);
    
    CloseHandle(remoteThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    if (exitCode == 0) {
        std::cerr << "[-] DLL injection failed" << std::endl;
        return false;
    }
    
    std::cout << "[+] DLL injected successfully" << std::endl;
    return true;
}

// Method C: DLL search-order hijacking (placeholder)
bool InjectMethodC(const wchar_t* targetDir, const wchar_t* dllName) {
    std::wcout << L"[*] Method C: DLL hijacking (placeholder)" << std::endl;
    std::wcout << L"[*] Target directory: " << targetDir << std::endl;
    std::wcout << L"[*] DLL name: " << dllName << std::endl;
    std::cout << "[!] Method C not implemented yet" << std::endl;
    return false;
}

void PrintUsage() {
    std::cout << "BirriLauncher - DLL Injector" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Method A (spawn suspended):" << std::endl;
    std::cout << "    BirriLauncher -a <exe_path> <dll_path>" << std::endl;
    std::cout << "  Method B (inject running process):" << std::endl;
    std::cout << "    BirriLauncher -b <pid> <dll_path>" << std::endl;
    std::cout << "  Method C (DLL hijacking):" << std::endl;
    std::cout << "    BirriLauncher -c <target_dir> <dll_name>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }
    
    std::string method = argv[1];
    
    if (method == "-a" && argc >= 4) {
        // Method A
        wchar_t exePath[MAX_PATH];
        wchar_t dllPath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, exePath, MAX_PATH);
        MultiByteToWideChar(CP_ACP, 0, argv[3], -1, dllPath, MAX_PATH);
        
        if (!InjectMethodA(exePath, dllPath)) {
            std::cerr << "[-] Method A failed" << std::endl;
            return 1;
        }
    } else if (method == "-b" && argc >= 4) {
        // Method B
        DWORD pid = strtoul(argv[2], nullptr, 10);
        wchar_t dllPath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, argv[3], -1, dllPath, MAX_PATH);
        
        if (!InjectMethodB(pid, dllPath)) {
            std::cerr << "[-] Method B failed" << std::endl;
            return 1;
        }
    } else if (method == "-c" && argc >= 4) {
        // Method C
        wchar_t targetDir[MAX_PATH];
        wchar_t dllName[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, targetDir, MAX_PATH);
        MultiByteToWideChar(CP_ACP, 0, argv[3], -1, dllName, MAX_PATH);
        
        if (!InjectMethodC(targetDir, dllName)) {
            std::cerr << "[-] Method C failed" << std::endl;
            return 1;
        }
    } else {
        PrintUsage();
        return 1;
    }
    
    std::cout << "[+] Injection successful. Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
