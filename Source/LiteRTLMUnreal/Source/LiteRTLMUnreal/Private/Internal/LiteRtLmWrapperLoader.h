// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

#include "litert_lm_wrapper.h"

/**
 * Helper to dynamically load the LiteRT-LM shared library and resolve function pointers.
 */
class FLiteRtLmWrapperLoader
{
public:
    static bool LoadDll();
    static void UnloadDll();

    // Function pointers – signatures must match litert_lm_wrapper.h exactly
    typedef void* (*PN_CreateEngine)(LiteRtLm_Config);
    typedef void (*PN_DestroyEngine)(void*);
    typedef void (*PN_AppendUserMessage)(const char*);
    typedef void (*PN_AppendHistoryMessage)(const char*);
    typedef void (*PN_RunInference)(LiteRtLm_SamplingParams, LiteRtLmCallback, void*);
    typedef void (*PN_StopMessage)();
    typedef int (*PN_WaitUntilDone)(void*, int);
    typedef const char* (*PN_GetAvailableBackends)();
    typedef int (*PNGetKVCache)(void*, size_t*);
    typedef int (*PNSetKVCache)(const void*, size_t);

    static PN_CreateEngine CreateEngine;
    static PN_DestroyEngine DestroyEngine;
    static PN_AppendUserMessage AppendUserMessage;
    static PN_AppendHistoryMessage AppendHistoryMessage;
    static PN_RunInference RunInference;
    static PN_StopMessage StopMessage;
    static PN_WaitUntilDone WaitUntilDone;
    static PN_GetAvailableBackends GetAvailableBackends;
    static PNGetKVCache GetKVCache;
    static PNSetKVCache SetKVCache;

private:
    static void* DllHandle;
    static TArray<void*> PreloadedHandles;
    static void PreloadDependencyLibraries(const FString& LibraryDir);
};
