// Copyright (c) 2025-2026 Winyunq. All rights reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChatWithUnreal : ModuleRules
{
	public ChatWithUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		bUseUnity = false;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory + "/Public"
			}
		);
		if (System.IO.Directory.Exists(ModuleDirectory + "/Public"))
		{
			foreach (string SubDir in System.IO.Directory.GetDirectories(ModuleDirectory + "/Public", "*", System.IO.SearchOption.AllDirectories))
			{
				PublicIncludePaths.Add(SubDir);
			}
		}
		
		PrivateIncludePaths.AddRange(
			new string[] {
				ModuleDirectory + "/Private"
			}
		);
		if (System.IO.Directory.Exists(ModuleDirectory + "/Private"))
		{
			foreach (string SubDir in System.IO.Directory.GetDirectories(ModuleDirectory + "/Private", "*", System.IO.SearchOption.AllDirectories))
			{
				PrivateIncludePaths.Add(SubDir);
			}
		}
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"HTTP",
				"Json",
				"JsonUtilities",
				"Slate",
				"SlateCore",
				"UMG",
				"UnrealEd"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"ApplicationCore",
				"Projects",
				"AssetRegistry",
				"WorkspaceMenuStructure",
				"ImageWrapper"
			}
		);
	}
}
