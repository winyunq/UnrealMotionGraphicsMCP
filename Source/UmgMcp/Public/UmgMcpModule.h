#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUmgMcpModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FUmgMcpModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUmgMcpModule>("UmgMcp");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UmgMcp");
	}
}; 