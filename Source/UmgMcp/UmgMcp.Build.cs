// Copyright (c) 2025-2026 Winyunq. All rights reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UmgMcp : ModuleRules
{
	public UmgMcp(ReadOnlyTargetRules Target) : base(Target)
	{
		bool bWithFabServer = Directory.Exists(Path.Combine(ModuleDirectory, "Private", "FabServer"));
		bUseUnity = false;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDefinitions.Add("UMGMCP_EXPORTS=1");
		PublicDefinitions.Add(bWithFabServer ? "WITH_UMGMCP_FABSERVER=1" : "WITH_UMGMCP_FABSERVER=0");

		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory + "/Public"
			}
		);
		
		PrivateIncludePaths.AddRange(
			new string[] {
				ModuleDirectory + "/Private",
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"PhysicsCore",
				"UnrealEd",
				"UMG",
                "MovieScene",
				"MovieSceneTracks",
				"MaterialEditor",
                "Slate",
				"SlateCore"
			}
		);

		if (bWithFabServer)
		{
			PublicIncludePaths.Add(ModuleDirectory + "/Public/FabServer");
			PublicDependencyModuleNames.AddRange(new string[] { "LiteRTLMUnreal", "ChatWithUnreal" });
		}
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"ApplicationCore", // For ClipboardCopy
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"Projects",
				"AssetRegistry",
				"Settings",
				"UMGEditor",
				"WorkspaceMenuStructure",
				"MaterialEditor",
				"ImageWrapper",
				"Serialization"

			}
		);
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",
					"ToolMenus",
					"BlueprintEditorLibrary"
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
