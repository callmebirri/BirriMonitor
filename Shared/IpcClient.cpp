#include "IpcClient.h"
#include <cstring>

IpcClient::IpcClient() : m_pipe(INVALID_HANDLE_VALUE), m_connected(false) {
    InitializeCriticalSection(&m_cs);
}

IpcClient::~IpcClient() {
    Disconnect();
    DeleteCriticalSection(&m_cs);
}

bool IpcClient::Connect() {
    if (m_connected) return true;
    
    // Try to connect to named pipe (non-blocking)
    m_pipe = CreateFileW(
        PIPE_NAME,
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,  // Use overlapped for non-blocking
        nullptr
    );
    
    if (m_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    m_connected = true;
    return true;
}

void IpcClient::Disconnect() {
    if (m_connected) {
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
        m_connected = false;
    }
}

bool IpcClient::Send(const void* data, size_t size) {
    if (!m_connected || m_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    EnterCriticalSection(&m_cs);
    
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    
    DWORD written = 0;
    BOOL result = WriteFile(m_pipe, data, static_cast<DWORD>(size), &written, &overlapped);
    
    if (!result) {
        if (GetLastError() == ERROR_IO_PENDING) {
            // Wait for completion with timeout
            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, PIPE_TIMEOUT);
            if (waitResult == WAIT_OBJECT_0) {
                GetOverlappedResult(m_pipe, &overlapped, &written, FALSE);
                result = TRUE;
            }
        }
    }
    
    CloseHandle(overlapped.hEvent);
    LeaveCriticalSection(&m_cs);
    
    return result == TRUE;
}
