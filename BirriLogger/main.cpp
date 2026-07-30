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
#include "../Shared/IpcCommon.h"

// Undefine min/max macros from Windows headers to avoid conflicts with std::min/max
#undef min
#undef max

// Structure to hold a complete HTTP transaction (request + response)
struct HttpTransaction {
    std::string requestData;
    std::string responseData;
    std::string timestamp;
    uint64_t requestId;
    bool hasRequest;
    bool hasResponse;
    
    HttpTransaction() : requestId(0), hasRequest(false), hasResponse(false) {}
};

// Map to buffer transactions by request ID
std::map<uint64_t, HttpTransaction> g_transactions;

// Forward declarations
void PrintTransaction(const HttpTransaction& trans);

// Get current timestamp string
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

// Buffer incoming messages and pair request with response
void BufferMessage(const IpcMessage* msg) {
    uint64_t requestId = msg->metadata.requestId;
    
    if (msg->header.type == MessageType::HTTP_REQUEST) {
        HttpTransaction trans;
        trans.requestId = requestId;
        trans.timestamp = GetTimestamp();
        trans.hasRequest = true;
        
        // Store request data
        size_t dataSize = msg->header.length - sizeof(MessageHeader) - sizeof(HttpMetadata);
        if (dataSize > 0 && msg->data) {
            trans.requestData = std::string(reinterpret_cast<const char*>(msg->data), dataSize);
        }
        
        g_transactions[requestId] = trans;
        
    } else if (msg->header.type == MessageType::HTTP_RESPONSE) {
        auto it = g_transactions.find(requestId);
        if (it != g_transactions.end()) {
            it->second.hasResponse = true;
            
            // Store response data
            size_t dataSize = msg->header.length - sizeof(MessageHeader) - sizeof(HttpMetadata);
            if (dataSize > 0 && msg->data) {
                it->second.responseData = std::string(reinterpret_cast<const char*>(msg->data), dataSize);
            }
            
            // Print complete transaction
            PrintTransaction(it->second);
            
            // Remove from buffer
            g_transactions.erase(it);
        } else {
            // Response without matching request (shouldn't happen)
            HttpTransaction trans;
            trans.requestId = requestId;
            trans.timestamp = GetTimestamp();
            trans.hasResponse = true;
            trans.responseData = "Response without matching request";
            PrintTransaction(trans);
        }
    }
}

// Print a complete HTTP transaction
void PrintTransaction(const HttpTransaction& trans) {
    // Separator line
    std::cout << "\n────────────────────────────────────────────────────────\n";
    
    // Timestamp
    std::cout << trans.timestamp << "\n";
    
    // Request
    if (trans.hasRequest && !trans.requestData.empty()) {
        std::cout << "\n--> ";
        
        // Try to parse and format request
        std::istringstream requestStream(trans.requestData);
        std::string line;
        
        // First line: METHOD /path HTTP/x.x
        if (std::getline(requestStream, line)) {
            // Trim CRLF
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::cout << line << "\n";
        }
        
        // Headers
        while (std::getline(requestStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            std::cout << line << "\n";
        }
        
        // Body (if any)
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
    
    // Response
    if (trans.hasResponse && !trans.responseData.empty()) {
        std::cout << "\n<-- ";
        
        std::istringstream responseStream(trans.responseData);
        std::string line;
        
        // Status line
        if (std::getline(responseStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::cout << line << "\n";
        }
        
        // Headers
        while (std::getline(responseStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            std::cout << line << "\n";
        }
        
        // Body
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

// Forward declarations
void HandleMessage(const IpcMessage* msg);
void ParseAndPrintHttp(const IpcMessage* msg);

// Named pipe server for receiving messages from DLL
class IpcServer {
public:
    IpcServer() : m_pipe(INVALID_HANDLE_VALUE), m_running(false) {}
    ~IpcServer() { Stop(); }
    
    bool Start() {
        m_running = true;
        m_thread = std::thread(&IpcServer::ServerThread, this);
        return true;
    }
    
    void Stop() {
        m_running = false;
        if (m_pipe != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(m_pipe);
            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    
private:
    void ServerThread() {
        while (m_running) {
            // Create named pipe
            m_pipe = CreateNamedPipeW(
                PIPE_NAME,
                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,  // Max instances
                PIPE_BUFFER_SIZE,
                PIPE_BUFFER_SIZE,
                0,
                nullptr
            );
            
            if (m_pipe == INVALID_HANDLE_VALUE) {
                std::cerr << "Failed to create named pipe. Error: " << GetLastError() << std::endl;
                return;
            }
            
            // Wait for connection
            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            
            ConnectNamedPipe(m_pipe, &overlapped);
            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 5000);
            CloseHandle(overlapped.hEvent);
            
            if (waitResult == WAIT_OBJECT_0) {
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
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    std::cout << "[*] DLL disconnected" << std::endl;
                    break;
                }
                continue;
            }
            
            if (bytesRead < sizeof(MessageHeader)) {
                continue;
            }
            
            // Parse message
            const IpcMessage* msg = reinterpret_cast<const IpcMessage*>(buffer.data());
            
            // Validate magic
            if (msg->header.magic != 0xB17A) {
                std::cerr << "[!] Invalid message magic" << std::endl;
                continue;
            }
            
            HandleMessage(msg);
        }
    }
    
    HANDLE m_pipe;
    bool m_running;
    std::thread m_thread;
};

// Message handler
void HandleMessage(const IpcMessage* msg) {
    switch (msg->header.type) {
        case MessageType::HOOKS_READY:
            std::cout << "[*] Hooks ready (PID: " << msg->header.processId 
                      << ", TID: " << msg->header.threadId << ")" << std::endl;
            break;
            
        case MessageType::HTTP_REQUEST:
        case MessageType::HTTP_RESPONSE:
            // Buffer messages and pair them
            BufferMessage(msg);
            break;
            
        // Ignore all other message types (LOG_MESSAGE, ERROR_MESSAGE, RAW_SEND, etc.)
        default:
            break;
    }
}

// Helper: Check if data looks like HTTP
bool IsHttpData(const char* data, size_t size) {
    if (size < 5) return false;
    
    // Check for HTTP methods or HTTP response
    std::string start(data, (std::min)(size, (size_t)16));
    if (start.find("HTTP/") == 0) return true;  // Response
    if (start.find("GET ") == 0) return true;
    if (start.find("POST ") == 0) return true;
    if (start.find("PUT ") == 0) return true;
    if (start.find("DELETE ") == 0) return true;
    if (start.find("HEAD ") == 0) return true;
    if (start.find("OPTIONS ") == 0) return true;
    
    return false;
}

// Helper: Print hex dump for non-HTTP data
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

// Parse and print HTTP traffic - curl -i style
void ParseAndPrintHttp(const IpcMessage* msg) {
    const uint8_t* data = msg->data;
    size_t dataSize = msg->header.length - sizeof(MessageHeader) - sizeof(HttpMetadata);
    
    if (dataSize == 0 || !data) {
        return;
    }
    
    std::string httpData(reinterpret_cast<const char*>(data), dataSize);
    
    if (msg->header.type == MessageType::HTTP_REQUEST) {
        std::cout << "\n--> ";
        
        // Try to parse HTTP request from data
        // Format expected: METHOD /path?query HTTP/1.1\r\nHost: ...\r\n\r\nbody
        size_t firstLineEnd = httpData.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            std::string firstLine = httpData.substr(0, firstLineEnd);
            std::cout << firstLine << "\n";
            
            // Parse headers
            size_t pos = firstLineEnd + 2;
            while (pos < httpData.length()) {
                size_t lineEnd = httpData.find("\r\n", pos);
                if (lineEnd == std::string::npos || lineEnd == pos) {
                    // Empty line = end of headers
                    std::cout << "\n";
                    pos = (lineEnd == pos) ? lineEnd + 2 : pos;
                    break;
                }
                
                std::string headerLine = httpData.substr(pos, lineEnd - pos);
                std::cout << headerLine << "\n";
                pos = lineEnd + 2;
            }
            
            // Body
            if (pos < httpData.length()) {
                std::string body = httpData.substr(pos);
                if (!body.empty() && body != "\r\n") {
                    std::cout << body << "\n";
                }
            }
        } else {
            // No CRLF found, print raw
            std::cout << httpData << "\n";
        }
        
    } else if (msg->header.type == MessageType::HTTP_RESPONSE) {
        std::cout << "\n<-- ";
        
        // Parse HTTP response
        // Format: HTTP/1.1 200 OK\r\nHeaders\r\n\r\nBody
        size_t firstLineEnd = httpData.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            std::string statusLine = httpData.substr(0, firstLineEnd);
            std::cout << statusLine << "\n";
            
            // Parse headers
            size_t pos = firstLineEnd + 2;
            while (pos < httpData.length()) {
                size_t lineEnd = httpData.find("\r\n", pos);
                if (lineEnd == std::string::npos || lineEnd == pos) {
                    std::cout << "\n";
                    pos = (lineEnd == pos) ? lineEnd + 2 : pos;
                    break;
                }
                
                std::string headerLine = httpData.substr(pos, lineEnd - pos);
                std::cout << headerLine << "\n";
                pos = lineEnd + 2;
            }
            
            // Body
            if (pos < httpData.length()) {
                std::string body = httpData.substr(pos);
                if (!body.empty() && body != "\r\n") {
                    std::cout << body << "\n";
                }
            }
        } else {
            std::cout << httpData << "\n";
        }
    }
    
    std::cout << std::endl;
}

int main() {
    std::cout << "BirriLogger - Network Traffic Monitor" << std::endl;
    std::cout << "Waiting for DLL connection..." << std::endl;
    
    IpcServer server;
    if (!server.Start()) {
        std::cerr << "Failed to start IPC server" << std::endl;
        return 1;
    }
    
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    server.Stop();
    return 0;
}
