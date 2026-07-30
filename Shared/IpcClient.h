#pragma once

#include "IpcCommon.h"
#include <windows.h>
#include <string>

// IPC Client for DLL -> Logger communication via Named Pipe
class IpcClient {
public:
    IpcClient();
    ~IpcClient();
    
    // Connect to logger process
    bool Connect();
    
    // Disconnect
    void Disconnect();
    
    // Send message (non-blocking from hook side)
    bool Send(const void* data, size_t size);
    
    // Check if connected
    bool IsConnected() const { return m_connected; }
    
private:
    HANDLE m_pipe;
    bool m_connected;
    CRITICAL_SECTION m_cs;
};
