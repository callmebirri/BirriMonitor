#pragma once

#include "IpcCommon.h"
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>


class IpcClient;


class HookEngine {
public:
    HookEngine();
    ~HookEngine();


    bool Initialize();


    void Shutdown();


    bool SendMessage(MessageType type, const HttpMetadata* metadata,
                    const void* data, size_t dataSize);


    bool IsReady() const;


    bool IsInHook() const;
    void SetInHook(bool inHook);

private:
    friend class IpcClient;

    static DWORD WINAPI InitThread(void*);

    bool InstallHooks();


    void UninstallHooks();


    bool SendHandshake();


    std::unique_ptr<IpcClient> m_ipcClient;


    DWORD m_tlsIndex;


    std::mutex m_cs;
};


extern HookEngine g_hookEngine;
extern std::atomic<bool> g_hooksReady;
