#include "pch.h"
#include "HookEngine.h"
#include "WinHttpHooks.h"
#include <windows.h>
#include <winhttp.h>
#include <MinHook.h>
#include <string>


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

static WinHttpOpenRequest_t Original_WinHttpOpenRequest = nullptr;
static WinHttpSendRequest_t Original_WinHttpSendRequest = nullptr;
static WinHttpReceiveResponse_t Original_WinHttpReceiveResponse = nullptr;
static WinHttpReadData_t Original_WinHttpReadData = nullptr;
static WinHttpQueryHeaders_t Original_WinHttpQueryHeaders = nullptr;


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
        HttpMetadata metadata;
        metadata.isRequest = true;
        metadata.connectionId = (uint64_t)hConnect;
        metadata.requestId = (uint64_t)result;

        std::wstring logMsg = L"WinHttpOpenRequest: ";
        if (lpszVerb) logMsg += lpszVerb;
        logMsg += L" ";
        if (lpszObjectName) logMsg += lpszObjectName;

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

    if (g_hookEngine.IsReady() && lpOptional && dwOptionalLength > 0) {
        HttpMetadata metadata;
        metadata.isRequest = true;
        metadata.requestId = (uint64_t)hRequest;

        g_hookEngine.SendMessage(MessageType::HTTP_REQUEST, &metadata, lpOptional, dwOptionalLength);
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
        HttpMetadata metadata;
        metadata.isRequest = false;
        metadata.requestId = (uint64_t)hRequest;

        g_hookEngine.SendMessage(MessageType::LOG_MESSAGE, &metadata,
                                "WinHttpReceiveResponse: Success", 28);
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

    if (result && g_hookEngine.IsReady() && lpBuffer && lpdwNumberOfBytesRead && *lpdwNumberOfBytesRead > 0) {
        HttpMetadata metadata;
        metadata.isRequest = false;
        metadata.requestId = (uint64_t)hRequest;

        g_hookEngine.SendMessage(MessageType::HTTP_RESPONSE, &metadata, lpBuffer, *lpdwNumberOfBytesRead);
    }

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

    if (!pOpenRequest || !pSendRequest || !pReceiveResponse || !pReadData || !pQueryHeaders) {
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
