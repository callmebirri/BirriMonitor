#include "pch.h"
#include "HookEngine.h"
#include "IpcClient.h"
#include "WinHttpHooks.h"
#include <cstring>
#include <cassert>
#include <memory>
#include <atomic>


HookEngine g_hookEngine;


static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_initInProgress{false};
std::atomic<bool> g_hooksReady{false};


DWORD WINAPI HookEngine::InitThread(void*) {
    g_initInProgress.store(true, std::memory_order_release);

    DWORD tlsIndex = TlsAlloc();
    if (tlsIndex == TLS_OUT_OF_INDEXES) {
        g_initInProgress.store(false, std::memory_order_release);
        return 0;
    }

    g_hookEngine.m_tlsIndex = tlsIndex;

    g_hookEngine.m_ipcClient = std::make_unique<IpcClient>();
    g_hookEngine.m_ipcClient->Connect();

    if (!InstallWinHttpHooks()) {
        g_initInProgress.store(false, std::memory_order_release);
        return 0;
    }

    g_hooksReady.store(true, std::memory_order_release);
    g_initialized.store(true, std::memory_order_release);
    g_initInProgress.store(false, std::memory_order_release);

    g_hookEngine.SendHandshake();
    return 0;
}


HookEngine::HookEngine()
    : m_tlsIndex(TLS_OUT_OF_INDEXES)
{
}


HookEngine::~HookEngine() {
    Shutdown();
}


bool HookEngine::Initialize() {
    if (g_initialized.load(std::memory_order_acquire)) return true;
    if (g_initInProgress.load(std::memory_order_acquire)) return false;

    HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)InitThread, nullptr, 0, nullptr);
    if (!hThread) return false;
    CloseHandle(hThread);

    return true;
}


void HookEngine::Shutdown() {
    if (!g_initialized.load(std::memory_order_acquire)) return;

    g_hooksReady.store(false, std::memory_order_release);

    UninstallWinHttpHooks();

    std::lock_guard<std::mutex> lock(m_cs);
    if (m_ipcClient) {
        m_ipcClient->Disconnect();
        m_ipcClient.reset();
    }

    if (m_tlsIndex != TLS_OUT_OF_INDEXES) {
        TlsFree(m_tlsIndex);
        m_tlsIndex = TLS_OUT_OF_INDEXES;
    }

    g_initialized.store(false, std::memory_order_release);
}


bool HookEngine::SendMessage(MessageType type, const HttpMetadata* metadata,
                           const void* data, size_t dataSize) {
    if (!g_initialized.load(std::memory_order_acquire) || !g_hooksReady.load(std::memory_order_acquire)) {
        return false;
    }

    IpcClient* client = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_cs);
        client = m_ipcClient.get();
    }
    if (!client || !client->IsConnected()) return false;

    bool truncated = false;
    if (dataSize > MAX_MESSAGE_SIZE) {
        dataSize = MAX_MESSAGE_SIZE;
        truncated = true;
    }

    HttpMetadata metaCopy;
    if (metadata) {
        metaCopy = *metadata;
    } else {
        memset(&metaCopy, 0, sizeof(HttpMetadata));
    }
    metaCopy.truncated = truncated;

    auto buffer = SerializeIpcMessage(type, &metaCopy, data, dataSize,
                                      GetWallTimestamp());

    return client->EnqueueMessage(buffer.data(), buffer.size());
}


bool HookEngine::IsReady() const {
    return g_hooksReady.load(std::memory_order_acquire);
}


bool HookEngine::IsInHook() const {
    if (m_tlsIndex == TLS_OUT_OF_INDEXES) return false;
    return TlsGetValue(m_tlsIndex) == (LPVOID)1;
}


void HookEngine::SetInHook(bool inHook) {
    if (m_tlsIndex != TLS_OUT_OF_INDEXES) {
        TlsSetValue(m_tlsIndex, inHook ? (LPVOID)1 : (LPVOID)0);
    }
}


bool HookEngine::InstallHooks() {
    if (!InstallWinHttpHooks()) {
        return false;
    }
    return true;
}


void HookEngine::UninstallHooks() {
    UninstallWinHttpHooks();
}


bool HookEngine::SendHandshake() {
    HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, HOOKS_READY_EVENT_NAME);
    if (hEvent) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }

    return SendMessage(MessageType::HOOKS_READY, nullptr, nullptr, 0);
}
