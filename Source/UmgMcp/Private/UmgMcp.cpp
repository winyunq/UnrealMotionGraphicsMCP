// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "UmgMcp.h"
#include "Bridge/UmgMcpConfig.h"
#include "Debug/SUmgMcpDebugPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

DEFINE_LOG_CATEGORY(LogUmgMcp);

#define LOCTEXT_NAMESPACE "FUmgMcpModule"

static const FName UmgMcpDebugTabName("UmgMcpDebug");

void FUmgMcpModule::StartupModule()
{
    UE_LOG(LogUmgMcp, Warning, TEXT("UMG agent Start!"));
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcp is running at default port: %d"), MCP_SERVER_PORT_DEFAULT);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UmgMcpDebugTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(SUmgMcpDebugPanel)
            ];
    }))
    .SetDisplayName(LOCTEXT("UmgMcpDebugTabTitle", "UMG MCP Debug Console"))
    .SetTooltipText(LOCTEXT("UmgMcpDebugTooltip", "Inspect and simulate raw AI protocol messages"))
    .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
}

void FUmgMcpModule::ShutdownModule()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UmgMcpDebugTabName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUmgMcpModule, UmgMcp)
