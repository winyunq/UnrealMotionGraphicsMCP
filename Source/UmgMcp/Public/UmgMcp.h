// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUmgMcp, Log, All);

/**
 * @brief The main module class for the UmgMcp plugin.
 *
 * This class implements the IModuleInterface and is the primary entry point for the plugin
 * when it's loaded by the engine. Its main responsibilities in `StartupModule()` are to
 * initialize and start the UUmgMcpBridge server and register any necessary callbacks.
 * In `ShutdownModule()`, it cleans up and stops the server.
 */
class FUmgMcpModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

#if WITH_UMGMCP_FABSERVER
    DECLARE_DELEGATE_RetVal(TSharedRef<class SWidget>, FOnSpawnChatWindow);
    static FOnSpawnChatWindow OnSpawnChatWindow;


    /** Register menus and toolbar buttons */
    void RegisterMenus();

private:
    /** Callback for spawning the plugin tab */
    TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

    /** Handlers for Asset Editor Integration */
    void Initialize();
    void OnAssetEditorOpened(UObject* Asset);
    void RegisterAssetEditorMenu(class IAssetEditorInstance* Instance);
    void RegisterAssetEditorFactory(UObject* Asset, TSharedPtr<class FUmgMcpChatTabFactory> Factory);

private:
	FDelegateHandle PostEngineInitHandle;
#endif

public:
    /**
     * Singleton-like access to this module's interface.
     * Beware of calling this during the shutdown phase, though.
     */
    static inline FUmgMcpModule& Get()
    {
        return FModuleManager::LoadModuleChecked< FUmgMcpModule >("UmgMcp");
    }

    /**
     * Checks whether this module is loaded and ready to use.
     */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("UmgMcp");
    }
};
