#include "pch.h"
#include "HookEngine.h"
#include "WinHttpHooks.h"
#include <winhttp.h>
#include <MinHook.h>
#include <string>
#include <map>
#include <mutex>


typedef HINTERNET (WINAPI* WinHttpOpenRequest_t)(
    HINTERNET hConnect,
    LPCWSTR lpszVerb,
    LPCWSTR lpszObjectName,
    LPCWSTR lpszVersion,
    LPCWSTR lpszReferrer,
    LPCWSTR* lplpszAcceptTypes,
    DWORD dwFlags
);

typedef BOOL (WINAPI* WinHttpSendRequest_t)(
    HINTERNET hRequest,
    LPCWSTR lpszHeaders,
    DWORD dwHeadersLength,
    LPVOID lpOptional,
    DWORD dwOptionalLength,
    DWORD dwTotalLength,
    DWORD_PTR dwContext
);

typedef BOOL (WINAPI* WinHttpReceiveResponse_t)(
    HINTERNET hRequest,
    LPVOID lpReserved
);

typedef BOOL (WINAPI* WinHttpReadData_t)(
    HINTERNET hRequest,
    LPVOID lpBuffer,
    DWORD dwNumberOfBytesToRead,
    LPDWORD lpdwNumberOfBytesRead
);

typedef BOOL (WINAPI* WinHttpQueryHeaders_t)(
    HINTERNET hRequest,
    DWORD dwInfoLevel,
    LPCWSTR lpszName,
    LPVOID lpBuffer,
    LPDWORD lpdwBufferLength,
    LPDWORD lpdwIndex
);

typedef BOOL (WINAPI* WinHttpCloseHandle_t)(
    HINTERNET hInternet
);


static WinHttpOpenRequest_t Original_WinHttpOpenRequest = nullptr;
static WinHttpSendRequest_t Original_WinHttpSendRequest = nullptr;
static WinHttpReceiveResponse_t Original_WinHttpReceiveResponse = nullptr;
static WinHttpReadData_t Original_WinHttpReadData = nullptr;
static WinHttpQueryHeaders_t Original_WinHttpQueryHeaders = nullptr;
static WinHttpCloseHandle_t Original_WinHttpCloseHandle = nullptr;


struct RequestContext {
    uint64_t uniqueId;
    std::wstring verb;
    std::wstring objectName;
    std::wstring headers;
};


static std::map<HINTERNET, RequestContext> g_requestContexts;
static std::mutex g_requestContextsMutex;
static std::atomic<uint64_t> g_nextUniqueId{1};


static uint64_t AllocUniqueId() {
    return g_nextUniqueId.fetch_add(1, std::memory_order_relaxed);
}


HINTERNET WINAPI Hooked_WinHttpOpenRequest(
    HINTERNET hConnect,
    LPCWSTR lpszVerb,
    LPCWSTR lpszObjectName,
    LPCWSTR lpszVersion,
    LPCWSTR lpszReferrer,
    LPCWSTR* lplpszAcceptTypes,
    DWORD dwFlags
) {
    if (g_hookEngine.IsInHook()) {
        return Original_WinHttpOpenRequest(hConnect, lpszVerb, lpszObjectName,
                                          lpszVersion, lpszReferrer,
                                          lplpszAcceptTypes, dwFlags);
    }

    g_hookEngine.SetInHook(true);

    HINTERNET result = Original_WinHttpOpenRequest(hConnect, lpszVerb, lpszObjectName,
                                                   lpszVersion, lpszReferrer,
                                                   lplpszAcceptTypes, dwFlags);

    if (result && g_hookEngine.IsReady()) {
        uint64_t uid = AllocUniqueId();
        RequestContext ctx;
        ctx.uniqueId = uid;
        ctx.verb = lpszVerb ? lpszVerb : L"GET";
        ctx.objectName = lpszObjectName ? lpszObjectName : L"";

        {
            std::lock_guard<std::mutex> lock(g_requestContextsMutex);
            g_requestContexts[result] = std::move(ctx);
        }

        HttpMetadata metadata;
        metadata.isRequest = true;
        metadata.connectionId = (uint64_t)hConnect;
        metadata.requestId = uid;

        wcsncpy_s(metadata.url, lpszObjectName ? lpszObjectName : L"", _TRUNCATE);

        std::wstring logMsg = L"WinHttpOpenRequest: ";
        logMsg += ctx.verb;
        logMsg += L" ";
        logMsg += ctx.objectName;

        g_hookEngine.SendMessage(MessageType::LOG_MESSAGE, &metadata,
                                logMsg.c_str(), (logMsg.length() + 1) * sizeof(wchar_t));
    }

    g_hookEngine.SetInHook(false);
    return result;
}


BOOL WINAPI Hooked_WinHttpSendRequest(
    HINTERNET hRequest,
    LPCWSTR lpszHeaders,
    DWORD dwHeadersLength,
    LPVOID lpOptional,
    DWORD dwOptionalLength,
    DWORD dwTotalLength,
    DWORD_PTR dwContext
) {
    if (g_hookEngine.IsInHook()) {
        return Original_WinHttpSendRequest(hRequest, lpszHeaders, dwHeadersLength,
                                          lpOptional, dwOptionalLength,
                                          dwTotalLength, dwContext);
    }

    g_hookEngine.SetInHook(true);

    if (g_hookEngine.IsReady()) {
        std::lock_guard<std::mutex> lock(g_requestContextsMutex);
        auto it = g_requestContexts.find(hRequest);
        if (it != g_requestContexts.end()) {
            if (lpszHeaders) {
                if (dwHeadersLength == (DWORD)-1) {
                    it->second.headers = lpszHeaders;
                } else {
                    it->second.headers.assign(lpszHeaders, dwHeadersLength / sizeof(wchar_t));
                }
            }
        }
    }

    if (g_hookEngine.IsReady() && lpOptional && dwOptionalLength > 0) {
        uint64_t uid = 0;
        {
            std::lock_guard<std::mutex> lock(g_requestContextsMutex);
            auto it = g_requestContexts.find(hRequest);
            if (it != g_requestContexts.end()) {
                uid = it->second.uniqueId;
            }
        }

        if (uid != 0) {
            HttpMetadata metadata;
            metadata.isRequest = true;
            metadata.requestId = uid;
            g_hookEngine.SendMessage(MessageType::HTTP_REQUEST, &metadata, lpOptional, dwOptionalLength);
        }
    }

    BOOL result = Original_WinHttpSendRequest(hRequest, lpszHeaders, dwHeadersLength,
                                              lpOptional, dwOptionalLength,
                                              dwTotalLength, dwContext);

    g_hookEngine.SetInHook(false);
    return result;
}


BOOL WINAPI Hooked_WinHttpReceiveResponse(
    HINTERNET hRequest,
    LPVOID lpReserved
) {
    if (g_hookEngine.IsInHook()) {
        return Original_WinHttpReceiveResponse(hRequest, lpReserved);
    }

    g_hookEngine.SetInHook(true);

    BOOL result = Original_WinHttpReceiveResponse(hRequest, lpReserved);

    if (result && g_hookEngine.IsReady()) {
        uint64_t uid = 0;
        {
            std::lock_guard<std::mutex> lock(g_requestContextsMutex);
            auto it = g_requestContexts.find(hRequest);
            if (it != g_requestContexts.end()) {
                uid = it->second.uniqueId;
            }
        }

        if (uid != 0) {
            HttpMetadata metadata;
            metadata.isRequest = false;
            metadata.requestId = uid;

            g_hookEngine.SendMessage(MessageType::LOG_MESSAGE, &metadata,
                                    "WinHttpReceiveResponse: Success", 28);
        }
    }

    g_hookEngine.SetInHook(false);
    return result;
}


BOOL WINAPI Hooked_WinHttpReadData(
    HINTERNET hRequest,
    LPVOID lpBuffer,
    DWORD dwNumberOfBytesToRead,
    LPDWORD lpdwNumberOfBytesRead
) {
    if (g_hookEngine.IsInHook()) {
        return Original_WinHttpReadData(hRequest, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
    }

    g_hookEngine.SetInHook(true);

    BOOL result = Original_WinHttpReadData(hRequest, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);

    DWORD lastErr = GetLastError();

    if ((result || lastErr == ERROR_MORE_DATA) && g_hookEngine.IsReady() && lpBuffer && lpdwNumberOfBytesRead && *lpdwNumberOfBytesRead > 0) {
        uint64_t uid = 0;
        {
            std::lock_guard<std::mutex> lock(g_requestContextsMutex);
            auto it = g_requestContexts.find(hRequest);
            if (it != g_requestContexts.end()) {
                uid = it->second.uniqueId;
            }
        }

        if (uid != 0) {
            HttpMetadata metadata;
            metadata.isRequest = false;
            metadata.requestId = uid;
            g_hookEngine.SendMessage(MessageType::HTTP_RESPONSE, &metadata, lpBuffer, *lpdwNumberOfBytesRead);
        }
    }

    g_hookEngine.SetInHook(false);
    return result;
}


BOOL WINAPI Hooked_WinHttpCloseHandle(
    HINTERNET hInternet
) {
    if (g_hookEngine.IsInHook()) {
        return Original_WinHttpCloseHandle(hInternet);
    }

    g_hookEngine.SetInHook(true);


    {
        std::lock_guard<std::mutex> lock(g_requestContextsMutex);
        g_requestContexts.erase(hInternet);
    }

    BOOL result = Original_WinHttpCloseHandle(hInternet);

    g_hookEngine.SetInHook(false);
    return result;
}


BOOL WINAPI Hooked_WinHttpQueryHeaders(
    HINTERNET hRequest,
    DWORD dwInfoLevel,
    LPCWSTR lpszName,
    LPVOID lpBuffer,
    LPDWORD lpdwBufferLength,
    LPDWORD lpdwIndex
) {
    if (g_hookEngine.IsInHook()) {
        return Original_WinHttpQueryHeaders(hRequest, dwInfoLevel, lpszName,
                                           lpBuffer, lpdwBufferLength, lpdwIndex);
    }

    g_hookEngine.SetInHook(true);

    BOOL result = Original_WinHttpQueryHeaders(hRequest, dwInfoLevel, lpszName,
                                              lpBuffer, lpdwBufferLength, lpdwIndex);

    g_hookEngine.SetInHook(false);
    return result;
}


bool InstallWinHttpHooks() {

    if (MH_Initialize() != MH_OK) {
        return false;
    }


    HMODULE hWinHttp = GetModuleHandleW(L"winhttp.dll");
    if (!hWinHttp) {
        hWinHttp = LoadLibraryW(L"winhttp.dll");
    }
    if (!hWinHttp) {
        MH_Uninitialize();
        return false;
    }


    void* pOpenRequest = (void*)GetProcAddress(hWinHttp, "WinHttpOpenRequest");
    void* pSendRequest = (void*)GetProcAddress(hWinHttp, "WinHttpSendRequest");
    void* pReceiveResponse = (void*)GetProcAddress(hWinHttp, "WinHttpReceiveResponse");
    void* pReadData = (void*)GetProcAddress(hWinHttp, "WinHttpReadData");
    void* pQueryHeaders = (void*)GetProcAddress(hWinHttp, "WinHttpQueryHeaders");
    void* pCloseHandle = (void*)GetProcAddress(hWinHttp, "WinHttpCloseHandle");

    if (!pOpenRequest || !pSendRequest || !pReceiveResponse || !pReadData || !pQueryHeaders || !pCloseHandle) {
        MH_Uninitialize();
        return false;
    }


    if (MH_CreateHook(pOpenRequest, &Hooked_WinHttpOpenRequest, (void**)&Original_WinHttpOpenRequest) != MH_OK) {
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(pSendRequest, &Hooked_WinHttpSendRequest, (void**)&Original_WinHttpSendRequest) != MH_OK) {
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(pReceiveResponse, &Hooked_WinHttpReceiveResponse, (void**)&Original_WinHttpReceiveResponse) != MH_OK) {
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(pReadData, &Hooked_WinHttpReadData, (void**)&Original_WinHttpReadData) != MH_OK) {
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(pQueryHeaders, &Hooked_WinHttpQueryHeaders, (void**)&Original_WinHttpQueryHeaders) != MH_OK) {
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(pCloseHandle, &Hooked_WinHttpCloseHandle, (void**)&Original_WinHttpCloseHandle) != MH_OK) {
        MH_Uninitialize();
        return false;
    }


    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        MH_Uninitialize();
        return false;
    }

    return true;
}


void UninstallWinHttpHooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
