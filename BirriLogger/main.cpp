#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <thread>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <chrono>
#include <iomanip>
#include <mutex>
#include "../Shared/IpcCommon.h"


#undef min
#undef max


struct HttpTransaction {
    std::string requestData;
    std::string responseData;
    std::string timestamp;
    std::wstring url;
    uint64_t requestId;
    bool hasRequest;
    bool hasResponse;

    HttpTransaction() : requestId(0), hasRequest(false), hasResponse(false) {}
};


std::map<uint64_t, HttpTransaction> g_transactions;
std::mutex g_transactionsMutex;


void PrintTransaction(const HttpTransaction& trans);


std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_s(&tm, &time_t_now);

    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
    return oss.str();
}


void BufferMessage(const IpcMessageWire* msg, size_t bytesRead) {
    if (bytesRead < sizeof(MessageHeader) + sizeof(HttpMetadata)) return;

    uint64_t requestId = msg->metadata.requestId;

    size_t headerLen = msg->header.length;
    if (headerLen > bytesRead) return;

    size_t dataSize = headerLen - sizeof(MessageHeader) - sizeof(HttpMetadata);
    if (dataSize > MAX_MESSAGE_SIZE) dataSize = MAX_MESSAGE_SIZE;

    const uint8_t* dataPtr = msg->data;
    if (dataSize > 0 && dataPtr) {

        std::lock_guard<std::mutex> lock(g_transactionsMutex);

        if (msg->header.type == MessageType::HTTP_REQUEST) {
            HttpTransaction trans;
            trans.requestId = requestId;
            trans.timestamp = GetTimestamp();
            trans.hasRequest = true;
            trans.url = msg->metadata.url;

            trans.requestData = std::string(reinterpret_cast<const char*>(dataPtr), dataSize);

            g_transactions[requestId] = trans;

        } else if (msg->header.type == MessageType::HTTP_RESPONSE) {
            auto it = g_transactions.find(requestId);
            if (it != g_transactions.end()) {
                it->second.hasResponse = true;

                it->second.responseData += std::string(reinterpret_cast<const char*>(dataPtr), dataSize);

                PrintTransaction(it->second);

                g_transactions.erase(it);
            } else {

                HttpTransaction trans;
                trans.requestId = requestId;
                trans.timestamp = GetTimestamp();
                trans.hasResponse = true;
                trans.responseData = "Response without matching request";
                PrintTransaction(trans);
            }
        }
    }
}


void PrintTransaction(const HttpTransaction& trans) {

    std::cout << "\n────────────────────────────────────────────────────────\n";

    std::cout << trans.timestamp << "\n";

    if (!trans.url.empty()) {
        std::wcout << L"URL: " << trans.url << L"\n";
    }

    if (trans.hasRequest && !trans.requestData.empty()) {
        std::cout << "\n--> ";

        std::istringstream requestStream(trans.requestData);
        std::string line;

        if (std::getline(requestStream, line)) {

            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::cout << line << "\n";
        }

        while (std::getline(requestStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            std::cout << line << "\n";
        }

        std::string body;
        while (std::getline(requestStream, line)) {
            body += line + "\n";
        }
        if (!body.empty()) {
            std::cout << "\n" << body;
        }
    } else {
        std::cout << "\n--> (no request data)\n";
    }

    if (trans.hasResponse && !trans.responseData.empty()) {
        std::cout << "\n<-- ";

        std::istringstream responseStream(trans.responseData);
        std::string line;

        if (std::getline(responseStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::cout << line << "\n";
        }

        while (std::getline(responseStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            std::cout << line << "\n";
        }

        std::string body;
        while (std::getline(responseStream, line)) {
            body += line + "\n";
        }
        if (!body.empty()) {
            std::cout << "\n" << body;
        }
    } else {
        std::cout << "\n<-- (pending...)\n";
    }

    std::cout << "\n────────────────────────────────────────────────────────\n" << std::endl;
}


void HandleMessage(const IpcMessageWire* msg, size_t bytesRead);


class IpcServer {
public:
    IpcServer() : m_pipe(INVALID_HANDLE_VALUE), m_running(false), m_stopEvent(nullptr) {}
    ~IpcServer() { Stop(); }

    bool Start() {
        m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!m_stopEvent) return false;

        m_running = true;
        m_thread = std::thread(&IpcServer::ServerThread, this);
        return true;
    }

    void Stop() {
        m_running = false;
        if (m_stopEvent) {
            SetEvent(m_stopEvent);
        }
        if (m_pipe != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(m_pipe);
            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
        if (m_thread.joinable()) {
            m_thread.join();
        }
        if (m_stopEvent) {
            CloseHandle(m_stopEvent);
            m_stopEvent = nullptr;
        }
    }

private:
    void ServerThread() {
        while (m_running) {

            m_pipe = CreateNamedPipeW(
                PIPE_NAME,
                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                PIPE_UNLIMITED,
                PIPE_BUFFER_SIZE,
                PIPE_BUFFER_SIZE,
                0,
                nullptr
            );

            if (m_pipe == INVALID_HANDLE_VALUE) {
                std::cerr << "Failed to create named pipe. Error: " << GetLastError() << std::endl;
                return;
            }

            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            if (!overlapped.hEvent) {
                CloseHandle(m_pipe);
                m_pipe = INVALID_HANDLE_VALUE;
                return;
            }

            ConnectNamedPipe(m_pipe, &overlapped);

            HANDLE waitHandles[2] = { overlapped.hEvent, m_stopEvent };
            DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            CloseHandle(overlapped.hEvent);

            if (waitResult == WAIT_OBJECT_0 + 1) {

                CancelIo(m_pipe);
                CloseHandle(m_pipe);
                m_pipe = INVALID_HANDLE_VALUE;
                break;
            }

            if (waitResult == WAIT_OBJECT_0) {
                DWORD dummy = 0;
                if (!GetOverlappedResult(m_pipe, &overlapped, &dummy, TRUE)) {

                }

                std::cout << "[*] DLL connected" << std::endl;
                ReceiveMessages();
                DisconnectNamedPipe(m_pipe);
            }

            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
    }

    void ReceiveMessages() {
        std::vector<uint8_t> buffer(PIPE_BUFFER_SIZE);

        while (m_running) {
            DWORD bytesRead = 0;
            BOOL result = ReadFile(m_pipe, buffer.data(), PIPE_BUFFER_SIZE, &bytesRead, nullptr);

            if (!result) {
                DWORD err = GetLastError();
                if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                    std::cout << "[*] DLL disconnected" << std::endl;
                    break;
                }
                continue;
            }

            if (bytesRead < sizeof(MessageHeader)) {
                continue;
            }

            const IpcMessageWire* msg = DeserializeIpcMessage(buffer.data());

            if (msg->header.magic != 0xB17A) {
                std::cerr << "[!] Invalid message magic" << std::endl;
                continue;
            }

            uint32_t declaredLength = msg->header.length;
            if (declaredLength < sizeof(MessageHeader) || declaredLength > bytesRead) {
                std::cerr << "[!] Invalid message length: " << declaredLength
                          << " (bytesRead=" << bytesRead << ")" << std::endl;
                continue;
            }

            if (declaredLength > PIPE_BUFFER_SIZE) {
                std::cerr << "[!] Message too large: " << declaredLength << std::endl;
                continue;
            }

            HandleMessage(msg, bytesRead);
        }
    }

    HANDLE m_pipe;
    bool m_running;
    std::thread m_thread;
    HANDLE m_stopEvent;
};


void HandleMessage(const IpcMessageWire* msg, size_t bytesRead) {
    switch (msg->header.type) {
        case MessageType::HOOKS_READY:
            std::cout << "[*] Hooks ready (PID: " << msg->header.processId
                      << ", TID: " << msg->header.threadId << ")" << std::endl;
            break;

        case MessageType::HTTP_REQUEST:
        case MessageType::HTTP_RESPONSE:

            BufferMessage(msg, bytesRead);
            break;


        default:
            break;
    }
}


bool IsHttpData(const char* data, size_t size) {
    if (size < 5) return false;

    std::string start(data, (std::min)(size, (size_t)16));
    if (start.find("HTTP/") == 0) return true;
    if (start.find("GET ") == 0) return true;
    if (start.find("POST ") == 0) return true;
    if (start.find("PUT ") == 0) return true;
    if (start.find("DELETE ") == 0) return true;
    if (start.find("HEAD ") == 0) return true;
    if (start.find("OPTIONS ") == 0) return true;

    return false;
}


void PrintHexDump(const uint8_t* data, size_t size) {
    const size_t bytesPerLine = 16;
    for (size_t i = 0; i < size; i += bytesPerLine) {
        printf("  %04zx: ", i);
        for (size_t j = 0; j < bytesPerLine; j++) {
            if (i + j < size) {
                printf("%02X ", data[i + j]);
            } else {
                printf("   ");
            }
        }
        printf(" ");
        for (size_t j = 0; j < bytesPerLine; j++) {
            if (i + j < size) {
                uint8_t c = data[i + j];
                printf("%c", (c >= 32 && c <= 126) ? c : '.');
            }
        }
        printf("\n");
    }
}


static bool g_consoleCtrlHandled = false;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        g_consoleCtrlHandled = true;
        return TRUE;
    }
    return FALSE;
}

int main() {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    std::cout << "BirriLogger - Network Traffic Monitor" << std::endl;
    std::cout << "Waiting for DLL connection..." << std::endl;

    IpcServer server;
    if (!server.Start()) {
        std::cerr << "Failed to start IPC server" << std::endl;
        return 1;
    }

    std::cout << "Press Enter or Ctrl+C to exit..." << std::endl;
    std::cin.get();

    server.Stop();
    return 0;
}
