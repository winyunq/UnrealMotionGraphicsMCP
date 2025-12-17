#include "UmgMcp.h"
#include "FabServer/SUmgMcpChatWindow.h"
#include "FabServer/UmgMcpSettings.h"
#include "FabServer/UmgMcpSettingsDetails.h"
#include "UmgMcpStyle.h"
#include "Internationalization/Culture.h"
#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FUmgMcpModule"

DEFINE_LOG_CATEGORY(LogUmgMcp);

static const FName UmgMcpTabName("UmgMcpChat");

void FUmgMcpModule::StartupModule()
{
    // Initialize Style
    FUmgMcpStyle::Initialize();
    FUmgMcpStyle::ReloadTextures();

	// Register the tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UmgMcpTabName, FOnSpawnTab::CreateRaw(this, &FUmgMcpModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FUmgMcpTabTitle", "UMG AI Assistant"))
		.SetMenuType(ETabSpawnerMenuType::Enabled);

    // Helper for Localization
    auto GetLocalizedSettingsName = []() -> FText {
        FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
        if (Culture.StartsWith("zh"))
        {
            return FText::FromString(TEXT("Unreal Motion Graphics MCP")); 
        }
        return LOCTEXT("UmgMcpSettingsName", "Unreal Motion Graphics MCP");
    };

    auto GetLocalizedSettingsDesc = []() -> FText {
        FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
        if (Culture.StartsWith("zh"))
        {
            return FText::FromString(TEXT("配置 UMG AI 助手"));
        }
        return LOCTEXT("UmgMcpSettingsDescription", "Configure the UMG AI Assistant");
    };

    // Register Detail Customization
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(
        "UmgMcpSettings",
        FOnGetDetailCustomizationInstance::CreateStatic(&FUmgMcpSettingsDetails::MakeInstance)
    );

    // Register Settings in Editor Preferences
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->RegisterSettings("Editor", "Plugins", "UmgMcp",
            GetLocalizedSettingsName(),
            GetLocalizedSettingsDesc(),
            GetMutableDefault<UUmgMcpSettings>()
        );
    }

    // Register a menu entry in the Level Editor's "Window" menu (or Tools)
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUmgMcpModule::RegisterMenus));
}

void FUmgMcpModule::ShutdownModule()
{
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings("Editor", "Plugins", "UmgMcp");
    }

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UmgMcpTabName);
    
    // Shutdown Style
    FUmgMcpStyle::Shutdown();
}

TSharedRef<SDockTab> FUmgMcpModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SUmgMcpChatWindow)
		];
}

void FUmgMcpModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

    // Helper for Localization - Now used as a delegate
    auto GetLocalizedLabel = []() -> FText {
        FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
        if (Culture.StartsWith("zh"))
        {
            return FText::FromString(TEXT("UMG AI 助手"));
        }
        return LOCTEXT("UmgMcpChatTabTitle", "UMG AI Assistant");
    };

    // Helper for Tooltip Localization
    auto GetLocalizedTooltip = []() -> FText {
        FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
        if (Culture.StartsWith("zh"))
        {
            return FText::FromString(TEXT("打开 UMG AI 助手聊天窗口"));
        }
        return LOCTEXT("UmgMcpChatTooltipText", "Open the UMG AI Assistant Chat Window");
    };

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			
            // Use AddMenuEntry with TAttribute for dynamic text
            FToolMenuEntry& Entry = Section.AddMenuEntry(
				"UmgMcpChat",
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(GetLocalizedLabel)),
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(GetLocalizedTooltip)),
				FSlateIcon(), // TODO: Add Icon
				FUIAction(FExecuteAction::CreateLambda([](){
                    FGlobalTabmanager::Get()->TryInvokeTab(UmgMcpTabName);
                }))
			);
		}
	}
    
    // Add to UMG Editor Toolbar (AssetEditor.WidgetBlueprintEditor.ToolBar)
    {
        // Note: The section name "Asset" or "Designer" might vary, but "Asset" is common.
        // We try to find or add a section.
        UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("AssetEditor.WidgetBlueprintEditor.ToolBar");
        {
            FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("UmgMcpTools");
            
            // Load Icon
            // Assuming the icon is in Resources folder and we can access it via Plugin's base dir or a style set.
            // For simplicity in this raw C++ file without a StyleSet class, we might need to rely on standard icons or a simple path if FSlateImageBrush is used elsewhere.
            // However, FSlateIcon requires a StyleSetName and a StyleName.
            // Since we haven't set up a full StyleSet, we will use a standard icon for now but ensure the text is localized.
            // To use our custom icon properly, we would need to register a Slate Style Set.
            // For this iteration, we will stick to the "Icons.Help" or similar, but with correct localization.
            // If the user *really* wants the custom icon, we need to implement a Style Class.
            
            // Let's stick to the requested Localization fix first.
            
            FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
                "UmgMcpChatToolbar",
                FUIAction(FExecuteAction::CreateLambda([](){
                    FGlobalTabmanager::Get()->TryInvokeTab(UmgMcpTabName);
                })),
                TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(GetLocalizedLabel)),
                TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(GetLocalizedTooltip)),
                FSlateIcon(FUmgMcpStyle::GetStyleSetName(), "UmgMcp.PluginIcon")
            );
            
            Section.AddEntry(Entry);
        }
    }
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUmgMcpModule, UmgMcp)
