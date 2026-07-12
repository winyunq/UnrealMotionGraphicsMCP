// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Internal/LiteRtLmWrapperLoader.h"
#include "LiteRtLmUnrealApi.h" // Include to use LogLiteRtLm
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

void* FLiteRtLmWrapperLoader::DllHandle = nullptr;
TArray<void*> FLiteRtLmWrapperLoader::PreloadedHandles;

FLiteRtLmWrapperLoader::PN_CreateEngine FLiteRtLmWrapperLoader::CreateEngine = nullptr;
FLiteRtLmWrapperLoader::PN_DestroyEngine FLiteRtLmWrapperLoader::DestroyEngine = nullptr;
FLiteRtLmWrapperLoader::PN_AppendUserMessage FLiteRtLmWrapperLoader::AppendUserMessage = nullptr;
FLiteRtLmWrapperLoader::PN_AppendHistoryMessage FLiteRtLmWrapperLoader::AppendHistoryMessage = nullptr;
FLiteRtLmWrapperLoader::PN_RunInference FLiteRtLmWrapperLoader::RunInference = nullptr;
FLiteRtLmWrapperLoader::PN_StopMessage FLiteRtLmWrapperLoader::StopMessage = nullptr;
FLiteRtLmWrapperLoader::PN_WaitUntilDone FLiteRtLmWrapperLoader::WaitUntilDone = nullptr;
FLiteRtLmWrapperLoader::PN_GetAvailableBackends FLiteRtLmWrapperLoader::GetAvailableBackends = nullptr;
FLiteRtLmWrapperLoader::PNGetKVCache FLiteRtLmWrapperLoader::GetKVCache = nullptr;
FLiteRtLmWrapperLoader::PNSetKVCache FLiteRtLmWrapperLoader::SetKVCache = nullptr;

/**
 * Returns the platform-specific wrapper library name.
 */
static FString GetWrapperLibraryName()
{
#if PLATFORM_WINDOWS
    return TEXT("litert_lm_wrapper.dll");
#elif PLATFORM_MAC
    return TEXT("liblitert_lm_wrapper.dylib");
#elif PLATFORM_LINUX
    return TEXT("liblitert_lm_wrapper.so");
#else
    return TEXT("litert_lm_wrapper");
#endif
}

/**
 * Pre-load dependency libraries from the same directory as the main wrapper library.
 */
void FLiteRtLmWrapperLoader::PreloadDependencyLibraries(const FString& LibraryDir)
{
#if PLATFORM_WINDOWS
    static const TCHAR* DepNames[] = {
        TEXT("dxcompiler.dll"),
        TEXT("dxil.dll"),
        TEXT("libLiteRt.dll"),
        TEXT("libLiteRtWebGpuAccelerator.dll"),
        TEXT("libLiteRtTopKWebGpuSampler.dll"),
        TEXT("libGemmaModelConstraintProvider.dll"),
    };
#elif PLATFORM_MAC
    static const TCHAR* DepNames[] = {
        TEXT("libLiteRt.dylib"),
        TEXT("libLiteRtWebGpuAccelerator.dylib"),
        TEXT("libLiteRtTopKWebGpuSampler.dylib"),
        TEXT("libLiteRtMetalAccelerator.dylib"),
        TEXT("libGemmaModelConstraintProvider.dylib"),
    };
#elif PLATFORM_LINUX
    static const TCHAR* DepNames[] = {
        TEXT("libLiteRt.so"),
        TEXT("libLiteRtWebGpuAccelerator.so"),
        TEXT("libLiteRtTopKWebGpuSampler.so"),
        TEXT("libGemmaModelConstraintProvider.so"),
    };
#else
    static const TCHAR* DepNames[] = {};
#endif

    for (const TCHAR* DepName : DepNames)
    {
        FString DepPath = FPaths::Combine(LibraryDir, DepName);
        if (FPaths::FileExists(DepPath))
        {
            void* H = FPlatformProcess::GetDllHandle(*DepPath);
            if (H)
            {
                FLiteRtLmWrapperLoader::PreloadedHandles.Add(H);
            }
            UE_LOG(LogLiteRtLm, Log, TEXT("Pre-load %s: %s"), DepName, H ? TEXT("OK") : TEXT("FAILED"));
        }
    }
}

/**
 * Recursively search for a file within a directory.
 */
static FString FindFileRecursive(const FString& StartDir, const FString& TargetFileName)
{
    TArray<FString> FoundFiles;
    IFileManager::Get().FindFilesRecursive(FoundFiles, *StartDir, *TargetFileName, true, false, false);
    
    for (const FString& FilePath : FoundFiles)
    {
        // Ignore files in Intermediate or metadata directories
        if (!FilePath.Contains(TEXT("Intermediate")) && !FilePath.Contains(TEXT(".git")))
        {
            return FilePath;
        }
    }
    return TEXT("");
}

bool FLiteRtLmWrapperLoader::LoadDll()
{
    if (DllHandle) return true;

    const FString TargetLibraryName = GetWrapperLibraryName();

    // 1. Primary path: BaseDir (Standard for Packaged builds or after UBT staging)
    FString LibraryPath = FPaths::Combine(FPlatformProcess::BaseDir(), TargetLibraryName);

    if (!FPaths::FileExists(LibraryPath))
    {
        UE_LOG(LogLiteRtLm, Warning, TEXT("Shared library not found at BaseDir: %s. Performing global adaptive search..."), *LibraryPath);

        // 2. Adaptive Search: Scan project and engine plugins directories
        // This removes dependency on a specific plugin name ("LiteRT-LM-Unreal")
        TArray<FString> SearchRoots;
        SearchRoots.Add(FPaths::ProjectPluginsDir());
        SearchRoots.Add(FPaths::EnginePluginsDir());

        for (const FString& Root : SearchRoots)
        {
            LibraryPath = FindFileRecursive(Root, TargetLibraryName);
            if (!LibraryPath.IsEmpty()) break;
        }

        if (LibraryPath.IsEmpty() || !FPaths::FileExists(LibraryPath))
        {
            UE_LOG(LogLiteRtLm, Error, TEXT("Shared library %s not found in any plugin directory."), *TargetLibraryName);
            return false;
        }
    }

    UE_LOG(LogLiteRtLm, Log, TEXT("Loading shared library from: %s"), *LibraryPath);

    // Pre-load dependency libraries from the same directory to resolve implicit linking.
    const FString LibraryDir = FPaths::GetPath(LibraryPath);
    
#if PLATFORM_WINDOWS
    // 永久注册当前插件依赖目录到虚幻进程搜索路径，保障底层多模态加载在任何生命周期调用 LoadLibrary 时均能顺利自动检索到所有加速动态库
    FPlatformProcess::AddDllDirectory(*LibraryDir);
#endif
    
    FPlatformProcess::PushDllDirectory(*LibraryDir);
    PreloadDependencyLibraries(LibraryDir);

    DllHandle = FPlatformProcess::GetDllHandle(*LibraryPath);
    FPlatformProcess::PopDllDirectory(*LibraryDir);

    if (!DllHandle)
    {
        UE_LOG(LogLiteRtLm, Error, TEXT("GetDllHandle failed for: %s (Check if dependencies are missing)"), *LibraryPath);
        return false;
    }

    // Resolve Symbols
    CreateEngine = (PN_CreateEngine)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_CreateEngine"));
    DestroyEngine = (PN_DestroyEngine)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_DestroyEngine"));
    AppendUserMessage = (PN_AppendUserMessage)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_AppendUserMessage"));
    AppendHistoryMessage = (PN_AppendHistoryMessage)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_AppendHistoryMessage"));
    RunInference = (PN_RunInference)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_RunInference"));
    StopMessage = (PN_StopMessage)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_StopMessage"));
    WaitUntilDone = (PN_WaitUntilDone)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_WaitUntilDone"));
    GetAvailableBackends = (PN_GetAvailableBackends)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_GetAvailableBackends"));
    GetKVCache = (PNGetKVCache)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_GetKVCache"));
    SetKVCache = (PNSetKVCache)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_SetKVCache"));

    if (!CreateEngine || !RunInference)
    {
        UE_LOG(LogLiteRtLm, Error, TEXT("Failed to resolve key symbols."));
        UnloadDll();
        return false;
    }

    UE_LOG(LogLiteRtLm, Log, TEXT("Shared library loaded. All symbols resolved."));
    return true;
}

void FLiteRtLmWrapperLoader::UnloadDll()
{
    if (DllHandle)
    {
        FPlatformProcess::FreeDllHandle(DllHandle);
        DllHandle = nullptr;
    }

    // 物理级彻底卸载：逆序释放预加载的所有加速依赖 DLL 句柄，斩断 DXGI/DirectML 等在 DLL 内持有的显存关联
    for (int32 i = PreloadedHandles.Num() - 1; i >= 0; --i)
    {
        if (PreloadedHandles[i])
        {
            FPlatformProcess::FreeDllHandle(PreloadedHandles[i]);
        }
    }
    PreloadedHandles.Empty();

#if PLATFORM_WINDOWS
    // 强力物理释放机制：在 Windows 系统上由于复杂隐式链接和引用计数，常规 FreeLibrary 可能不会真正卸载 DLL，
    // 采用 Windows-native 的 Module 循环强制释放直到其引用计数降为 0。
    {
        // 1. 释放主 DLL
        HMODULE Mod = ::GetModuleHandleW(TEXT("litert_lm_wrapper.dll"));
        if (Mod)
        {
            int32 Safety = 0;
            while (::GetModuleHandleW(TEXT("litert_lm_wrapper.dll")) != NULL && Safety < 100)
            {
                ::FreeLibrary(Mod);
                Safety++;
            }
        }

        // 2. 释放依赖 DLL
        static const TCHAR* DepNames[] = {
            TEXT("libGemmaModelConstraintProvider.dll"),
            TEXT("libLiteRtTopKWebGpuSampler.dll"),
            TEXT("libLiteRtWebGpuAccelerator.dll"),
            TEXT("libLiteRt.dll"),
            TEXT("dxil.dll"),
            TEXT("dxcompiler.dll")
        };
        for (const TCHAR* DepName : DepNames)
        {
            HMODULE DepMod = ::GetModuleHandleW(DepName);
            if (DepMod)
            {
                int32 DepSafety = 0;
                while (::GetModuleHandleW(DepName) != NULL && DepSafety < 100)
                {
                    ::FreeLibrary(DepMod);
                    DepSafety++;
                }
            }
        }
    }
#endif

    CreateEngine = nullptr;
    DestroyEngine = nullptr;
    AppendUserMessage = nullptr;
    AppendHistoryMessage = nullptr;
    RunInference = nullptr;
    StopMessage = nullptr;
    WaitUntilDone = nullptr;
    GetAvailableBackends = nullptr;
    GetKVCache = nullptr;
    SetKVCache = nullptr;
}
