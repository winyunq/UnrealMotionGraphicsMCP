// Copyright (c) 2025-2026 Winyunq. All rights reserved.
using UnrealBuildTool;
using System.IO;
using System.Collections.Generic;

public class LiteRTLMUnreal : ModuleRules
{
	public LiteRTLMUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		bUseUnity = false;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Projects", "Json" });

		// 1. Automatically find the third-party library directory (no longer hardcoded)
		string TargetLibName = "LiteRtLm";
		string ThirdPartyPath = FindLibraryDirectory(PluginDirectory, TargetLibName);

		if (!string.IsNullOrEmpty(ThirdPartyPath))
		{
			// Automatically configure include path
			string IncludePath = Path.Combine(ThirdPartyPath, "Include");
			if (Directory.Exists(IncludePath))
			{
				PublicIncludePaths.Add(IncludePath);
			}

			// 2. Automatically search and add library dependencies for the active platform.
			// Expected layout: Source/ThirdParty/LiteRtLm/Binaries/<PlatformName>/*
			string BinaryPlatformDirName = GetBinaryPlatformDirName(Target.Platform);
			string PlatformBinaryPath = Path.Combine(ThirdPartyPath, "Binaries", BinaryPlatformDirName);
			if (Directory.Exists(PlatformBinaryPath))
			{
				try
				{
					var AllFiles = Directory.EnumerateFiles(PlatformBinaryPath, "*.*", SearchOption.AllDirectories);
					foreach (string FilePath in AllFiles)
					{
						string FileName = Path.GetFileName(FilePath);
						string Extension = Path.GetExtension(FilePath).ToLower();

						if (IsRuntimeLibrary(Extension))
						{
							RuntimeDependencies.Add("$(BinaryOutputDir)/" + FileName, FilePath);
						}
						else if (IsLinkLibrary(Extension))
						{
							PublicAdditionalLibraries.Add(FilePath);
						}
					}
				}
				catch (System.Exception)
				{
					// Fail silently to prevent UBT crash, let the compiler handle missing dependencies
				}
			}
		}
	}

	private string GetBinaryPlatformDirName(UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.Mac)
		{
			return "Mac";
		}

		return Platform.ToString();
	}

	private bool IsRuntimeLibrary(string Extension)
	{
		return Extension == ".dll" || Extension == ".dylib" || Extension == ".so";
	}

	private bool IsLinkLibrary(string Extension)
	{
		return Extension == ".lib" || Extension == ".a";
	}

	/// <summary>
	/// Recursively finds a directory by name starting from a root path.
	/// </summary>
	private string FindLibraryDirectory(string StartDir, string TargetName)
	{
		if (string.IsNullOrEmpty(StartDir) || !Directory.Exists(StartDir)) return null;
		if (Path.GetFileName(StartDir).Equals(TargetName, System.StringComparison.OrdinalIgnoreCase)) return StartDir;

		try
		{
			foreach (var Dir in Directory.EnumerateDirectories(StartDir))
			{
				// Skip UBT intermediate and metadata directories to improve speed
				string DirName = Path.GetFileName(Dir);
				if (DirName == "Intermediate" || DirName == ".git" || DirName == "Saved" || DirName == "Binaries") continue;

				string Found = FindLibraryDirectory(Dir, TargetName);
				if (Found != null) return Found;
			}
		}
		catch { }
		return null;
	}
}
