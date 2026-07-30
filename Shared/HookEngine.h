#pragma once

#include "IpcCommon.h"
#include <vector>
#include <mutex>
#include <memory>


class IpcClient;


class HookEngine {
public:
    HookEngine();
    ~HookEngine();


    bool Initialize();


    void Shutdown();


    bool SendMessage(MessageType type, const HttpMetadata* metadata,
                    const void* data, size_t dataSize);


    bool IsReady() const { return m_initialized && m_hooksReady; }


    bool IsInHook() const;
    void SetInHook(bool inHook);

private:

    bool InstallHooks();


    void UninstallHooks();


    bool SendHandshake();


    std::unique_ptr<IpcClient> m_ipcClient;


    bool m_initialized;
    bool m_hooksReady;


    DWORD m_tlsIndex;


    CRITICAL_SECTION m_cs;
};


extern HookEngine g_hookEngine;


#define GUARD_HOOK() \
    if (g_hookEngine.IsInHook()) return; \
    ScopedHookGuard hookGuard;


class ScopedHookGuard {
public:
    ScopedHookGuard() { g_hookEngine.SetInHook(true); }
    ~ScopedHookGuard() { g_hookEngine.SetInHook(false); }
};
