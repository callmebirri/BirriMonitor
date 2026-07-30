#pragma once

#include "IpcCommon.h"
#include <windows.h>
#include <string>


class IpcClient {
public:
    IpcClient();
    ~IpcClient();


    bool Connect();


    void Disconnect();


    bool Send(const void* data, size_t size);


    bool IsConnected() const { return m_connected; }

private:
    HANDLE m_pipe;
    bool m_connected;
    CRITICAL_SECTION m_cs;
};
