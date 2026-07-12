// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SToolModeSelector.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

SToolModeSelector::~SToolModeSelector()
{
	GConfig->SetString(TEXT("UmgMcp.UIState"), TEXT("LastSelectedToolMode"), *CurrentTool, GEditorPerProjectIni);
}

void SToolModeSelector::Construct(const FArguments& InArgs)
{
	CurrentTool = TEXT("ALL");
	OnGetInteractionModeEvent = InArgs._OnGetInteractionMode;
	OnToolModeChangedEvent = InArgs._OnToolModeChanged;

	// 使用全局静态寻址（物理追溯理论），确保在任何目录下都能找到 Resources
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UmgMcp"));
	FString BaseDir = Plugin.IsValid() ? Plugin->GetBaseDir() : TEXT("");
	
	if (!BaseDir.IsEmpty())
	{
		FString ToolModesDir = FPaths::Combine(BaseDir, TEXT("Resources"), TEXT("ToolModes"));
		TArray<FString> FoundFiles;
		IFileManager::Get().FindFiles(FoundFiles, *(ToolModesDir / TEXT("*.json")), true, false);

		for (const FString& FileName : FoundFiles)
		{
			FString ToolName = FPaths::GetBaseFilename(FileName);
			Options.Add(MakeShared<FString>(ToolName));
		}
	}

	if (!Options.ContainsByPredicate([](const TSharedPtr<FString>& In) { return In->Equals(TEXT("none")); }))
	{
		Options.Add(MakeShared<FString>(TEXT("none")));
	}

	if (!Options.ContainsByPredicate([](const TSharedPtr<FString>& In) { return In->Equals(TEXT("ALL")); }))
	{
		Options.Add(MakeShared<FString>(TEXT("ALL")));
	}

	TSharedPtr<FString> InitialSelection = Options[0];
	FString SavedTool;
	if (GConfig->GetString(TEXT("UmgMcp.UIState"), TEXT("LastSelectedToolMode"), SavedTool, GEditorPerProjectIni))
	{
		for (const TSharedPtr<FString>& Option : Options)
		{
			if (Option.IsValid() && Option->Equals(SavedTool, ESearchCase::IgnoreCase))
			{
				InitialSelection = Option;
				CurrentTool = *Option;
				break;
			}
		}
	}

	ChildSlot
	[
		SAssignNew(ComboBox, SComboBox<TSharedPtr<FString>>)
		.Visibility(this, &SToolModeSelector::GetVisibilityBasedOnMode)
		.OptionsSource(&Options)
		.OnGenerateWidget(this, &SToolModeSelector::MakeOptionWidget)
		.OnSelectionChanged(this, &SToolModeSelector::OnSelectionChanged)
		.InitiallySelectedItem(InitialSelection)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
			.Padding(FMargin(8, 2))
			[
				SNew(STextBlock)
				.Text(this, &SToolModeSelector::GetToolDisplayTextAttr)
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			]
		]
	];
}

FText SToolModeSelector::GetToolDisplayTextAttr() const
{
	return FText::FromString(CurrentTool);
}

EVisibility SToolModeSelector::GetVisibilityBasedOnMode() const
{
	if (OnGetInteractionModeEvent.IsBound())
	{
		return OnGetInteractionModeEvent.Execute() == TEXT("Develop") ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

void SToolModeSelector::SyncWithInteractionMode(const FString& NewInteractionMode)
{
	FString TargetTool = CurrentTool;
	if (NewInteractionMode == TEXT("Task")) TargetTool = TEXT("Task_Master");
	else if (NewInteractionMode == TEXT("Chat")) TargetTool = TEXT("Chat");
	else if (NewInteractionMode == TEXT("Learning")) TargetTool = TEXT("Learning");

	for (const auto& Opt : Options)
	{
		if (Opt.IsValid() && *Opt == TargetTool)
		{
			ComboBox->SetSelectedItem(Opt);
			break;
		}
	}
}

TSharedRef<SWidget> SToolModeSelector::MakeOptionWidget(TSharedPtr<FString> Option)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*Option))
		.Margin(FMargin(8, 4));
}

void SToolModeSelector::OnSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		CurrentTool = *NewValue;
		GConfig->SetString(TEXT("UmgMcp.UIState"), TEXT("LastSelectedToolMode"), *CurrentTool, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);

		OnToolModeChangedEvent.ExecuteIfBound(CurrentTool);
	}
}
