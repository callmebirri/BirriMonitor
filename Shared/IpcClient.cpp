#include "pch.h"
#include "IpcClient.h"
#include <cstring>
#include <algorithm>


IpcClient::IpcClient()
    : m_pipe(INVALID_HANDLE_VALUE),
      m_connected(false),
      m_running(false)
{
    InitializeCriticalSection(&m_pipeCs);
}


IpcClient::~IpcClient() {
    Disconnect();
    DeleteCriticalSection(&m_pipeCs);
}


bool IpcClient::Connect() {
    if (m_connected.load(std::memory_order_acquire)) return true;

    HANDLE pipe = CreateFileW(
        PIPE_NAME,
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    EnterCriticalSection(&m_pipeCs);
    if (m_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipe);
    }
    m_pipe = pipe;
    LeaveCriticalSection(&m_pipeCs);

    m_connected.store(true, std::memory_order_release);

    if (!m_running) {
        m_running = true;
        m_ioThread = std::thread(&IpcClient::IoThread, this);
    }

    return true;
}


void IpcClient::Disconnect() {
    if (!m_connected.exchange(false, std::memory_order_acq_rel)) return;

    m_running = false;
    m_queueCv.notify_all();

    EnterCriticalSection(&m_pipeCs);
    if (m_pipe != INVALID_HANDLE_VALUE) {
        CancelIo(m_pipe);
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&m_pipeCs);

    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }
}


bool IpcClient::EnqueueMessage(const void* data, size_t size) {
    if (!m_connected.load(std::memory_order_acquire)) return false;

    if (size > PIPE_BUFFER_SIZE) return false;

    QueuedMessage msg;
    msg.data.resize(size);
    memcpy(msg.data.data(), data, size);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_queue.size() > 1000) {
            return false;
        }
        m_queue.push(std::move(msg));
    }
    m_queueCv.notify_one();
    return true;
}


void IpcClient::IoThread() {
    while (m_running) {

        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCv.wait(lock, [this]() { return !m_queue.empty() || !m_running; });

        if (!m_running) break;

        QueuedMessage msg = std::move(m_queue.front());
        m_queue.pop();
        lock.unlock();

        if (!m_connected.load(std::memory_order_acquire)) continue;

        EnterCriticalSection(&m_pipeCs);
        if (m_pipe == INVALID_HANDLE_VALUE) {
            LeaveCriticalSection(&m_pipeCs);
            continue;
        }

        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) {
            LeaveCriticalSection(&m_pipeCs);
            continue;
        }

        DWORD written = 0;
        BOOL result = WriteFile(m_pipe, msg.data.data(),
                               static_cast<DWORD>(msg.data.size()),
                               &written, &overlapped);

        if (!result) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, PIPE_TIMEOUT);
                if (waitResult == WAIT_OBJECT_0) {
                    GetOverlappedResult(m_pipe, &overlapped, &written, FALSE);
                } else {
                    CancelIo(m_pipe);
                }
            }
        }

        CloseHandle(overlapped.hEvent);
        LeaveCriticalSection(&m_pipeCs);
    }
}
