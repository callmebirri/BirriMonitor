#include "HookEngine.h"
#include "IpcClient.h"
#include <cstring>
#include <cassert>

// Global instance
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
    
    // Allocate TLS slot
    m_tlsIndex = TlsAlloc();
    if (m_tlsIndex == TLS_OUT_OF_INDEXES) {
        return false;
    }
    
    // Create IPC client
    m_ipcClient = std::make_unique<IpcClient>();
    
    // Try to connect to logger (non-blocking, will retry in SendMessage)
    m_ipcClient->Connect();
    
    // Install hooks
    if (!InstallHooks()) {
        return false;
    }
    
    m_initialized = true;
    
    // Send handshake (will queue if logger not ready)
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
    
    // Ensure connection
    if (!m_ipcClient->IsConnected()) {
        m_ipcClient->Connect();
        if (!m_ipcClient->IsConnected()) {
            return false;  // Logger not running
        }
    }
    
    // Calculate total size
    size_t totalSize = sizeof(MessageHeader) + sizeof(HttpMetadata) + dataSize;
    if (totalSize > PIPE_BUFFER_SIZE) {
        // Data too large, truncate or drop
        dataSize = PIPE_BUFFER_SIZE - sizeof(MessageHeader) - sizeof(HttpMetadata);
        totalSize = sizeof(MessageHeader) + sizeof(HttpMetadata) + dataSize;
    }
    
    // Allocate message buffer
    std::vector<uint8_t> buffer(totalSize);
    IpcMessage* msg = reinterpret_cast<IpcMessage*>(buffer.data());
    
    // Fill header
    msg->header.magic = 0xB17A;
    msg->header.length = static_cast<uint32_t>(totalSize);
    msg->header.type = type;
    msg->header.timestamp = GetCurrentTimestamp();
    msg->header.threadId = GetCurrentThreadId();
    msg->header.processId = GetCurrentProcessId();
    
    // Fill metadata
    if (metadata) {
        msg->metadata = *metadata;
    } else {
        memset(&msg->metadata, 0, sizeof(HttpMetadata));
    }
    
    // Copy data
    if (data && dataSize > 0) {
        memcpy(msg->data, data, dataSize);
    }
    
    // Send via IPC
    return m_ipcClient->Send(buffer.data(), totalSize);
}

bool HookEngine::IsInHook() const {
    if (m_tlsIndex == TLS_OUT_OF_INDEXES) return false;
    return reinterpret_cast<bool>(TlsGetValue(m_tlsIndex));
}

void HookEngine::SetInHook(bool inHook) {
    if (m_tlsIndex != TLS_OUT_OF_INDEXES) {
        TlsSetValue(m_tlsIndex, reinterpret_cast<LPVOID>(inHook));
    }
}

bool HookEngine::InstallHooks() {
    // Install WinHTTP hooks (Phase 1)
    if (!InstallWinHttpHooks()) {
        return false;
    }
    
    // TODO: Install other hooks (Winsock, WinINet, etc.)
    
    m_hooksReady = true;
    return true;
}

void HookEngine::UninstallHooks() {
    // Uninstall WinHTTP hooks
    UninstallWinHttpHooks();
    
    // TODO: Uninstall other hooks
    
    m_hooksReady = false;
}

bool HookEngine::SendHandshake() {
    // Send HOOKS_READY message
    return SendMessage(MessageType::HOOKS_READY, nullptr, nullptr, 0);
}
