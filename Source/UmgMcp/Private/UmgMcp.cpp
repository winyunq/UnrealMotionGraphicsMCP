#include "UmgMcp.h"
#if WITH_UMGMCP_FABSERVER
#include "FabServer/UmgMcpSettings.h"
#include "FabServer/UmgMcpSettingsDetails.h"
#include "FabServer/UmgMcpProviderEntryCustomization.h"

#include "UmgMcpStyle.h"
#include "SChatWindow.h"
#include "Internationalization/Culture.h"
#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "WidgetBlueprint.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "FileManage/UmgAttentionSubsystem.h"
//#include "FabServer/UmgMcpChatTabFactory.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"

// Include for FWidgetBlueprintEditor
#include "WidgetBlueprintEditor.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h" 
#include "Misc/MessageDialog.h"
#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"
#include "FabServer/AIProviders/IUmgMcpAiProvider.h"
#include "LiteRtLmUnrealApi.h"
#include "Debug/SUmgMcpDebugPanel.h"
#include "FabServer/AIProviders/Local/UmgMcpLiteRtLmAiProvider.h"
#define LOCTEXT_NAMESPACE "FUmgMcpModule"

DEFINE_LOG_CATEGORY(LogUmgMcp);

FUmgMcpModule::FOnSpawnChatWindow FUmgMcpModule::OnSpawnChatWindow;

static const FName UmgMcpTabName("UmgMcpChat");
static const FName UmgMcpDebugTabName("UmgMcpDebug");

// #include "SChatWindow.h" // Removed UI dependency

void FUmgMcpModule::StartupModule()
{
    // Initialize Style
    FUmgMcpStyle::Initialize();
    FUmgMcpStyle::ReloadTextures();

    // Register Detail Customization
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(
        "UmgMcpSettings",
        FOnGetDetailCustomizationInstance::CreateStatic(&FUmgMcpSettingsDetails::MakeInstance)
    );
 
    PropertyModule.RegisterCustomPropertyTypeLayout(
        "UmgMcpApiProviderEntry",
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FUmgMcpApiProviderEntryCustomization::MakeInstance)
    );


    // Register Settings in Editor Preferences
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->RegisterSettings("Editor", "Plugins", "UmgMcp",
            LOCTEXT("UmgMcpSettingsName", "Unreal Motion Graphics MCP"),
            LOCTEXT("UmgMcpSettingsDescription", "Configure the UMG AI Assistant"),
            GetMutableDefault<UUmgMcpSettings>()
        );
    }
    
    // Defer Editor Hooks until Engine Init is complete
    PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FUmgMcpModule::Initialize);
}

void FUmgMcpModule::ShutdownModule()
{
    FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UmgMcpTabName);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UmgMcpDebugTabName);

    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings("Editor", "Plugins", "UmgMcp");
    }
    
    // Shutdown Style
    FUmgMcpStyle::Shutdown();
}

#include "SMessageInteractionHub.h"
#include "Welcome/SChatWelcome.h"
#include "BottomBar/SBottomBar.h"
#include "BottomBar/SChatSendButton.h"
#include "MessageInteractionHub/Messages/SAgentResponseGroup.h"
#include "MessageInteractionHub/Messages/SUserMessageWidget.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "FabServer/ChatSystem/UmgMcpTaskSubsystem.h"
#include "MessageInteractionHub/Messages/STaskBeginNode.h"

void FUmgMcpModule::Initialize()
{
    // 重新绑定窗口生成委托，关联前后端，并在窗口构造出后主动加载历史会话列表
    OnSpawnChatWindow.BindLambda([]()
    {
        TSharedRef<SChatWindow> Window = SNew(SChatWindow);

        if (GEditor)
        {
            if (auto* TaskSys = GEditor->GetEditorSubsystem<UUmgMcpTaskSubsystem>())
            {
                TaskSys->BindChatInputEvents();
            }
        }

        // 1. 桥接顶部栏操作
        Window->OnShowHistory.BindLambda([]() {
            if (GEditor)
            {
                if (auto* ActiveMsgSys = GEditor->GetEditorSubsystem<UUmgMcpActiveMessageSubsystem>())
                {
                    ActiveMsgSys->ShowWelcomeWidget();
                }
            }
        });
        Window->OnNewConversation.BindLambda([]() {
            if (GEditor)
            {
                if (auto* ActiveMsgSys = GEditor->GetEditorSubsystem<UUmgMcpActiveMessageSubsystem>())
                {
                    ActiveMsgSys->ClearMessageHistory();
                }
            }
        });
        Window->OnSendClicked.BindLambda([]() {
            if (GEditor)
            {
                if (auto* ActiveMsgSys = GEditor->GetEditorSubsystem<UUmgMcpActiveMessageSubsystem>())
                {
                    ActiveMsgSys->ExecuteSendMessage();
                }
            }
        });
        Window->OnInterruptClicked.BindLambda([]() {
            if (GEditor)
            {
                if (auto* ActiveMsgSys = GEditor->GetEditorSubsystem<UUmgMcpActiveMessageSubsystem>())
                {
                    ActiveMsgSys->ReturnToUser();
                }
            }
        });
        Window->OnInteractionModeChanged.BindLambda([](const FString& NewMode) {
            if (GEditor)
            {
                if (auto* ActiveMsgSys = GEditor->GetEditorSubsystem<UUmgMcpActiveMessageSubsystem>())
                {
                    ActiveMsgSys->SetInteractionMode(NewMode);
                }
            }
        });

        // 2. 桥接历史列表中断与选中
        if (auto Hub = SMessageInteractionHub::Instance.Pin())
        {
            Hub->OnSessionSelectedEvent.BindLambda([](const FString& SessionId) {
                if (GEditor)
                {
                    if (auto* SessionManager = GEditor->GetEditorSubsystem<UUmgMcpSessionManagerSubsystem>())
                    {
                        SessionManager->ResumeSession(SessionId);
                    }
                }
            });
            Hub->OnSessionDeletedEvent.BindLambda([](const FString& SessionId) {
                if (GEditor)
                {
                    if (auto* SessionManager = GEditor->GetEditorSubsystem<UUmgMcpSessionManagerSubsystem>())
                    {
                        SessionManager->DeleteSession(SessionId);
                    }
                }
            });
        }

        // 4. 后端驱动 UI 初始装填最近历史列表
        UUmgMcpSessionManagerSubsystem::RefreshWelcomeHistoryList();
        return Window;
    });

    // Register Global Nomad Tab Spawner
    // This allows the window to outlive any specific UMG editor instance
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UmgMcpTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .OnCanCloseTab_Lambda([]() 
            {
                if (GEditor)
                {
                    if (UUmgMcpAiSubsystem* AiSubsystem = GEditor->GetEditorSubsystem<UUmgMcpAiSubsystem>())
                    {
                        TSharedPtr<IUmgMcpAiProvider> Provider = AiSubsystem->GetProviderForCurrentMode();
                        // 仅当当前使用的是本地大模型，并且模型已经物理装载加载到 GPU 时，才弹出释放显存确认提示
                        if (Provider.IsValid() && Provider->GetProviderName() == TEXT("LiteRtLmLocal") && FLiteRtLmUnrealApi::IsModelLoaded())
                        {
                            const FText Msg = LOCTEXT("UmgMcpConfirmCloseLocal", "The local LLM is currently running and occupying GPU memory. Would you like to unload it to free up VRAM for other tasks?\n\n[Yes] Unload model and close window\n[No] Close window and keep model loaded\n[Cancel] Keep window open");
                            const FText Title = LOCTEXT("UmgMcpConfirmCloseLocalTitle", "Unload Local Model");

                            EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNoCancel, Msg, Title);
                            if (Result == EAppReturnType::Yes)
                            {
                                // 用户选择“是”：直接在这里主动触发大模型物理卸载，连根拔起释放 DLL 与显存
                                StaticCastSharedPtr<FUmgMcpLiteRtLmAiProvider>(Provider)->UnloadModel();
                                return true; // 允许关闭
                            }
                            else if (Result == EAppReturnType::No)
                            {
                                // 用户选择“否”：只关闭窗口，让大模型继续热驻留在 GPU 中
                                return true; 
                            }
                            else
                            {
                                return false; // 拒绝关闭（取消操作）
                            }
                        }
                    }
                }
                return true; // 远程模型或未装载时直接允许关闭，无任何弹窗干扰
            })
            [
                FUmgMcpModule::OnSpawnChatWindow.IsBound() ? FUmgMcpModule::OnSpawnChatWindow.Execute() : SNullWidget::NullWidget
            ];
    }))
    .SetDisplayName(LOCTEXT("UmgMcpChatTabTitle", "UMG AI Assistant"))
    .SetTooltipText(LOCTEXT("UmgMcpChatTooltipText", "Open UMG AI Assistant"))
    .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
    .SetIcon(FSlateIcon(FUmgMcpStyle::GetStyleSetName(), "UmgMcp.PluginIcon"));

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
    .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
    .SetIcon(FSlateIcon(FUmgMcpStyle::GetStyleSetName(), "UmgMcp.PluginIcon"));

    // Still need to hook into Asset Editor opening to add the toolbar button
    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetSys = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetSys->OnAssetEditorOpened().AddRaw(this, &FUmgMcpModule::OnAssetEditorOpened);
        }
    }
}

// Map to keep factories alive
static TMap<TWeakObjectPtr<UObject>, TSharedPtr<FUmgMcpChatTabFactory>> OpenFactories;

void FUmgMcpModule::RegisterAssetEditorFactory(UObject* Asset, TSharedPtr<FUmgMcpChatTabFactory> Factory)
{
    // Clean up invalid
    for (auto It = OpenFactories.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
    
    if (Asset)
    {
        OpenFactories.Add(Asset, Factory);
    }
}

TSharedRef<SDockTab> FUmgMcpModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab);
}

void FUmgMcpModule::RegisterMenus()
{
}

void FUmgMcpModule::RegisterAssetEditorMenu(IAssetEditorInstance* Instance)
{
    // Extend the Widget Blueprint Toolbar
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("AssetEditor.WidgetBlueprintEditor.ToolBar");
    {
        FToolMenuSection& Section = Menu->FindOrAddSection("UmgMcpTools");
        
        FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
            "UmgMcpChatToolbar",
            FUIAction(
                FExecuteAction::CreateLambda([](){
                    // Single Global Nomad Tab invocation
                    FGlobalTabmanager::Get()->TryInvokeTab(UmgMcpTabName);
                })
            ),
            LOCTEXT("UmgMcpChatTabTitle", "UMG AI Assistant"),
            LOCTEXT("UmgMcpChatTooltipText", "Open UMG AI Assistant"),
            FSlateIcon(FUmgMcpStyle::GetStyleSetName(), "UmgMcp.PluginIcon")
        );
        Section.AddEntry(Entry);

        FToolMenuEntry DebugEntry = FToolMenuEntry::InitToolBarButton(
            "UmgMcpDebugToolbar",
            FUIAction(FExecuteAction::CreateLambda([]()
            {
                FGlobalTabmanager::Get()->TryInvokeTab(UmgMcpDebugTabName);
            })),
            LOCTEXT("UmgMcpDebugToolbarTitle", "MCP Debug"),
            LOCTEXT("UmgMcpDebugToolbarTooltip", "Open the raw MCP request/response console"),
            FSlateIcon(FUmgMcpStyle::GetStyleSetName(), "UmgMcp.PluginIcon")
        );
        Section.AddEntry(DebugEntry);
    }
}

void FUmgMcpModule::OnAssetEditorOpened(UObject* Asset)
{
    // Only need to ensure the toolbar button is added
    if (!Asset || (!Asset->IsA(UWidgetBlueprint::StaticClass()) && !Asset->IsA(UMaterial::StaticClass()) && !Asset->IsA(UMaterialInstance::StaticClass())))
    {
        return;
    }
    
    if (!GEditor) return;
    UAssetEditorSubsystem* AssetSys = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetSys) return;

    IAssetEditorInstance* Instance = AssetSys->FindEditorForAsset(Asset, false);
    if (Instance)
    {
        RegisterAssetEditorMenu(Instance);
    }
}

#undef LOCTEXT_NAMESPACE

#else

#include "Bridge/UmgMcpConfig.h"
#include "Debug/SUmgMcpDebugPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FUmgMcpModule"

DEFINE_LOG_CATEGORY(LogUmgMcp);

static const FName UmgMcpDebugTabName("UmgMcpDebug");

void FUmgMcpModule::StartupModule()
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcp core loaded; default bridge port is %d."), MCP_SERVER_PORT_DEFAULT);
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
#endif

IMPLEMENT_MODULE(FUmgMcpModule, UmgMcp)
