// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "HookEngine.h"
#include "WinHttpHooks.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Initialize hook engine
        if (!g_hookEngine.Initialize()) {
            return FALSE;
        }
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        // Shutdown hook engine
        g_hookEngine.Shutdown();
        break;
    }
    return TRUE;
}

