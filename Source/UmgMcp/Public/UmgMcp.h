#pragma once

#include "Modules/ModuleManager.h"

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