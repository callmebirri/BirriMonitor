#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>


constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\BirriMonitorPipe";


constexpr const wchar_t* HOOKS_READY_EVENT_NAME = L"Global\\BirriMonitorHooksReady";


constexpr DWORD PIPE_BUFFER_SIZE = 1024 * 64;


constexpr DWORD PIPE_TIMEOUT = 5000;


#ifndef PIPE_UNLIMITED
#define PIPE_UNLIMITED 255
#endif


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
};


struct HttpMetadata {
    uint64_t connectionId;
    uint64_t requestId;
    wchar_t url[2048];
    uint16_t statusCode;
    uint8_t method;
    bool isRequest;
    bool truncated;
};


struct IpcMessageWire {
    MessageHeader header;
    HttpMetadata metadata;
    uint8_t data[1];
};

#pragma pack(pop)


static_assert(sizeof(MessageHeader) <= 32, "MessageHeader too large");
static_assert(sizeof(HttpMetadata) <= 4120, "HttpMetadata too large");


constexpr size_t MAX_MESSAGE_SIZE = PIPE_BUFFER_SIZE - sizeof(MessageHeader) - sizeof(HttpMetadata);


inline uint64_t GetCurrentTimestamp() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
}


inline uint64_t GetWallTimestamp() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}


inline std::vector<uint8_t> SerializeIpcMessage(
    MessageType type,
    const HttpMetadata* metadata,
    const void* data,
    size_t dataSize,
    uint64_t timestamp)
{
    size_t totalSize = sizeof(MessageHeader) + sizeof(HttpMetadata) + dataSize;
    std::vector<uint8_t> buffer(totalSize);

    MessageHeader* hdr = reinterpret_cast<MessageHeader*>(buffer.data());
    hdr->magic = 0xB17A;
    hdr->length = static_cast<uint32_t>(totalSize);
    hdr->type = type;
    hdr->timestamp = timestamp;
    hdr->threadId = GetCurrentThreadId();
    hdr->processId = GetCurrentProcessId();

    HttpMetadata* meta = reinterpret_cast<HttpMetadata*>(buffer.data() + sizeof(MessageHeader));
    if (metadata) {
        *meta = *metadata;
    } else {
        memset(meta, 0, sizeof(HttpMetadata));
    }

    if (data && dataSize > 0) {
        memcpy(buffer.data() + sizeof(MessageHeader) + sizeof(HttpMetadata), data, dataSize);
    }

    return buffer;
}

inline const IpcMessageWire* DeserializeIpcMessage(const uint8_t* buffer) {
    return reinterpret_cast<const IpcMessageWire*>(buffer);
}
