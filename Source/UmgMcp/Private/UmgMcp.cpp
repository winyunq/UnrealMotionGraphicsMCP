#include "UmgMcp.h"
#include "UmgMcpConfig.h" // Include our config header

#define LOCTEXT_NAMESPACE "FUmgMcpModule"

void FUmgMcpModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogTemp, Warning, TEXT("UMG agent Start!"));

	// The UmgAttentionSubsystem is automatically initialized by the engine.

	// Use the hardcoded default port from UmgMcpConfig.h
	int32 Port = MCP_SERVER_PORT_DEFAULT;
	UE_LOG(LogTemp, Warning, TEXT("UmgMcp is running at: %d"), Port);
}

void FUmgMcpModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUmgMcpModule, UmgMcp)
