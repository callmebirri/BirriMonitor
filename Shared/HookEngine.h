#pragma once

#include "IpcCommon.h"
#include <vector>
#include <mutex>
#include <memory>

// Forward declarations
class IpcClient;

// Hook engine class - manages all hooks and IPC
class HookEngine {
public:
    HookEngine();
    ~HookEngine();
    
    // Initialize hook engine (call from DllMain DLL_PROCESS_ATTACH)
    bool Initialize();
    
    // Shutdown hook engine
    void Shutdown();
    
    // Send data via IPC
    bool SendMessage(MessageType type, const HttpMetadata* metadata, 
                    const void* data, size_t dataSize);
    
    // Check if hooks are ready
    bool IsReady() const { return m_initialized && m_hooksReady; }
    
    // Thread-local reentrancy guard
    bool IsInHook() const;
    void SetInHook(bool inHook);
    
private:
    // Install hooks
    bool InstallHooks();
    
    // Uninstall hooks
    void UninstallHooks();
    
    // Handshake with logger
    bool SendHandshake();
    
    // IPC client
    std::unique_ptr<IpcClient> m_ipcClient;
    
    // State
    bool m_initialized;
    bool m_hooksReady;
    
    // Thread-local storage for reentrancy guard
    DWORD m_tlsIndex;
    
    // Critical section for thread-safe operations
    CRITICAL_SECTION m_cs;
};

// Global instance (defined in HookEngine.cpp)
extern HookEngine g_hookEngine;

// Helper macro for hook functions
#define GUARD_HOOK() \
    if (g_hookEngine.IsInHook()) return; \
    ScopedHookGuard hookGuard;

// RAII guard for hook reentrancy
class ScopedHookGuard {
public:
    ScopedHookGuard() { g_hookEngine.SetInHook(true); }
    ~ScopedHookGuard() { g_hookEngine.SetInHook(false); }
};
