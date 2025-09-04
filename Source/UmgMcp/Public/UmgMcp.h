#pragma once

#include "Modules/ModuleManager.h"

// Define the concrete module class
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