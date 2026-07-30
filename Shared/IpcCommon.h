#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

// Named pipe name for IPC
constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\BirriMonitorPipe";

// Pipe buffer size
constexpr DWORD PIPE_BUFFER_SIZE = 1024 * 64;  // 64KB

// Timeout for pipe operations (ms)
constexpr DWORD PIPE_TIMEOUT = 5000;

// Message types
enum class MessageType : uint8_t {
    HOOKS_READY = 0,          // Handshake from DLL
    HTTP_REQUEST = 1,         // HTTP request data
    HTTP_RESPONSE = 2,        // HTTP response data
    RAW_SEND = 3,             // Raw send data (Winsock)
    RAW_RECV = 4,             // Raw recv data (Winsock)
    LOG_MESSAGE = 5,          // Log/debug message
    ERROR_MESSAGE = 6         // Error message
};

#pragma pack(push, 1)

// Message header (fixed size)
struct MessageHeader {
    uint32_t magic;           // Magic number for validation (0xB17A)
    uint32_t length;          // Total message length (including header)
    MessageType type;         // Message type
    uint64_t timestamp;       // Timestamp (QueryPerformanceCounter)
    DWORD threadId;           // Thread ID
    DWORD processId;          // Process ID
    
    MessageHeader() : magic(0xB17A), length(0), type(MessageType::LOG_MESSAGE), 
                      timestamp(0), threadId(0), processId(0) {}
};

// HTTP request/response metadata
struct HttpMetadata {
    uint64_t connectionId;    // Connection/socket handle
    uint64_t requestId;       // Request ID (for correlation)
    wchar_t url[2048];        // Full URL (for requests)
    uint16_t statusCode;      // HTTP status code (for responses)
    uint8_t method;           // HTTP method (0=GET, 1=POST, etc.)
    bool isRequest;           // true = request, false = response
    
    HttpMetadata() : connectionId(0), requestId(0), statusCode(0), 
                     method(0), isRequest(false) {
        url[0] = L'\0';
    }
};

// Complete message structure
struct IpcMessage {
    MessageHeader header;
    HttpMetadata metadata;
    uint8_t data[1];          // Variable-length data (headers + body)
    
    // Helper to get total size
    size_t GetTotalSize() const { return header.length; }
    
    // Helper to get data pointer
    uint8_t* GetData() { return const_cast<uint8_t*>(data); }
    const uint8_t* GetData() const { return data; }
};

#pragma pack(pop)

// Helper functions
inline uint64_t GetCurrentTimestamp() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
}

// Maximum message size (including header and metadata)
constexpr size_t MAX_MESSAGE_SIZE = PIPE_BUFFER_SIZE - sizeof(MessageHeader) - sizeof(HttpMetadata);
