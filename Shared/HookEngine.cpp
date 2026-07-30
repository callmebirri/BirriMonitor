#include "pch.h"
#include "HookEngine.h"
#include "IpcClient.h"
#include "WinHttpHooks.h"
#include <cstring>
#include <cassert>
#include <memory>


HookEngine g_hookEngine;

HookEngine::HookEngine() : m_initialized(false), m_hooksReady(false), m_tlsIndex(TLS_OUT_OF_INDEXES) {
    InitializeCriticalSection(&m_cs);
}

HookEngine::~HookEngine() {
    Shutdown();
    DeleteCriticalSection(&m_cs);
}

bool HookEngine::Initialize() {
    if (m_initialized) return true;


    m_tlsIndex = TlsAlloc();
    if (m_tlsIndex == TLS_OUT_OF_INDEXES) {
        return false;
    }


    m_ipcClient = std::make_unique<IpcClient>();


    m_ipcClient->Connect();


    if (!InstallHooks()) {
        return false;
    }

    m_initialized = true;


    SendHandshake();

    return true;
}

void HookEngine::Shutdown() {
    if (!m_initialized) return;

    UninstallHooks();

    if (m_ipcClient) {
        m_ipcClient->Disconnect();
        m_ipcClient.reset();
    }

    if (m_tlsIndex != TLS_OUT_OF_INDEXES) {
        TlsFree(m_tlsIndex);
        m_tlsIndex = TLS_OUT_OF_INDEXES;
    }

    m_initialized = false;
    m_hooksReady = false;
}

bool HookEngine::SendMessage(MessageType type, const HttpMetadata* metadata,
                           const void* data, size_t dataSize) {
    if (!m_initialized || !m_ipcClient) {
        return false;
    }


    if (!m_ipcClient->IsConnected()) {
        m_ipcClient->Connect();
        if (!m_ipcClient->IsConnected()) {
            return false;
        }
    }


    size_t totalSize = sizeof(MessageHeader) + sizeof(HttpMetadata) + dataSize;
    if (totalSize > PIPE_BUFFER_SIZE) {

        dataSize = PIPE_BUFFER_SIZE - sizeof(MessageHeader) - sizeof(HttpMetadata);
        totalSize = sizeof(MessageHeader) + sizeof(HttpMetadata) + dataSize;
    }


    std::vector<uint8_t> buffer(totalSize);
    IpcMessage* msg = reinterpret_cast<IpcMessage*>(buffer.data());


    msg->header.magic = 0xB17A;
    msg->header.length = static_cast<uint32_t>(totalSize);
    msg->header.type = type;
    msg->header.timestamp = GetCurrentTimestamp();
    msg->header.threadId = GetCurrentThreadId();
    msg->header.processId = GetCurrentProcessId();


    if (metadata) {
        msg->metadata = *metadata;
    } else {
        memset(&msg->metadata, 0, sizeof(HttpMetadata));
    }


    if (data && dataSize > 0) {
        memcpy(msg->data, data, dataSize);
    }


    return m_ipcClient->Send(buffer.data(), totalSize);
}

bool HookEngine::IsInHook() const {
    if (m_tlsIndex == TLS_OUT_OF_INDEXES) return false;
    return TlsGetValue(m_tlsIndex) != nullptr;
}

void HookEngine::SetInHook(bool inHook) {
    if (m_tlsIndex != TLS_OUT_OF_INDEXES) {
        TlsSetValue(m_tlsIndex, reinterpret_cast<LPVOID>(inHook));
    }
}

bool HookEngine::InstallHooks() {

    if (!InstallWinHttpHooks()) {
        return false;
    }



    m_hooksReady = true;
    return true;
}

void HookEngine::UninstallHooks() {

    UninstallWinHttpHooks();



    m_hooksReady = false;
}

bool HookEngine::SendHandshake() {

    return SendMessage(MessageType::HOOKS_READY, nullptr, nullptr, 0);
}
