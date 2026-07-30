#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>


constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\BirriMonitorPipe";


constexpr DWORD PIPE_BUFFER_SIZE = 1024 * 64;


constexpr DWORD PIPE_TIMEOUT = 5000;


enum class MessageType : uint8_t {
    HOOKS_READY = 0,
    HTTP_REQUEST = 1,
    HTTP_RESPONSE = 2,
    RAW_SEND = 3,
    RAW_RECV = 4,
    LOG_MESSAGE = 5,
    ERROR_MESSAGE = 6
};

#pragma pack(push, 1)


struct MessageHeader {
    uint32_t magic;
    uint32_t length;
    MessageType type;
    uint64_t timestamp;
    DWORD threadId;
    DWORD processId;

    MessageHeader() : magic(0xB17A), length(0), type(MessageType::LOG_MESSAGE),
                      timestamp(0), threadId(0), processId(0) {}
};


struct HttpMetadata {
    uint64_t connectionId;
    uint64_t requestId;
    wchar_t url[2048];
    uint16_t statusCode;
    uint8_t method;
    bool isRequest;

    HttpMetadata() : connectionId(0), requestId(0), statusCode(0),
                     method(0), isRequest(false) {
        url[0] = L'\0';
    }
};


struct IpcMessage {
    MessageHeader header;
    HttpMetadata metadata;
    uint8_t data[1];


    size_t GetTotalSize() const { return header.length; }


    uint8_t* GetData() { return const_cast<uint8_t*>(data); }
    const uint8_t* GetData() const { return data; }
};

#pragma pack(pop)


inline uint64_t GetCurrentTimestamp() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
}


constexpr size_t MAX_MESSAGE_SIZE = PIPE_BUFFER_SIZE - sizeof(MessageHeader) - sizeof(HttpMetadata);
