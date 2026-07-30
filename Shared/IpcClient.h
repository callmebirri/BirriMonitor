#pragma once

#include "IpcCommon.h"
#include <windows.h>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>


struct QueuedMessage {
    std::vector<uint8_t> data;
};


class IpcClient {
public:
    IpcClient();
    ~IpcClient();


    bool Connect();


    void Disconnect();


    bool EnqueueMessage(const void* data, size_t size);


    bool IsConnected() const { return m_connected.load(std::memory_order_acquire); }

private:
    void IoThread();

    HANDLE m_pipe;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_running;

    std::queue<QueuedMessage> m_queue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;

    std::thread m_ioThread;

    CRITICAL_SECTION m_pipeCs;
};
