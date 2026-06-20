// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgThemeSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUmgTheme, Log, All);

/**
 * Loads and caches design tokens from theme.json.
 * Supports @token references in set_widget_properties payloads.
 */
UCLASS()
class UMGMCP_API UUmgThemeSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Resolve "@color.primary" to a concrete value string (e.g. "0.1, 0.2, 0.8, 1.0"). */
	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Theme")
	FString ResolveToken(const FString& TokenRef) const;

	/** Recursively resolve @token strings inside a JSON object (in-place). */
	void ProcessJsonTokens(TSharedPtr<FJsonObject>& InOutJson, int32 Depth = 0) const;

	static constexpr int32 MaxTokenProcessDepth = 64;

	/** Reload theme cache from disk. Returns false when no theme file exists. */
	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Theme")
	bool ReloadThemeCache(const FString& OptionalAssetPath = TEXT(""));

	/** Merge patch object into theme and persist to disk, then reload cache. */
	bool ApplyThemePatch(const TSharedPtr<FJsonObject>& PatchObject, const FString& OptionalAssetPath = TEXT(""));

	/** Serialize cached theme to JSON string. */
	FString GetThemeJsonString() const;

	/** Unreal asset path for the active theme file. */
	FString GetActiveThemeAssetPath() const { return ActiveThemeAssetPath; }

private:
	FString ResolveThemeDiskPath(const FString& OptionalAssetPath) const;
	FString GetDefaultThemeAssetPath() const;
	bool LoadThemeFromDisk(const FString& DiskPath);
	TSharedPtr<FJsonValue> NavigateTokenPath(const FString& DotPath) const;
	TSharedPtr<FJsonValue> ResolveTokenValue(const FString& TokenRef) const;
	void ProcessJsonValue(TSharedPtr<FJsonValue>& Value, int32 Depth = 0) const;

	TSharedPtr<FJsonObject> CachedTheme;
	FString ActiveThemeAssetPath;
};
