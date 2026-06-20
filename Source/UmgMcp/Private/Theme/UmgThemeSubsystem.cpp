// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Theme/UmgThemeSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogUmgTheme);

namespace UmgThemeInternal
{
	static FString JsonValueToResolveString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return FString();
		}

		switch (Value->Type)
		{
		case EJson::String:
			return Value->AsString();
		case EJson::Number:
			return LexToString(Value->AsNumber());
		case EJson::Boolean:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		default:
			break;
		}

		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
		return Out;
	}

	static void DeepMergeJsonObject(const TSharedPtr<FJsonObject>& Patch, TSharedPtr<FJsonObject>& Target)
	{
		if (!Patch.IsValid() || !Target.IsValid())
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Patch->Values)
		{
			const TSharedPtr<FJsonObject>* PatchChildObject = nullptr;
			if (Field.Value->TryGetObject(PatchChildObject) && PatchChildObject && (*PatchChildObject).IsValid())
			{
				TSharedPtr<FJsonObject> TargetChild;
				const TSharedPtr<FJsonObject>* TargetChildObject = nullptr;
				if (Target->TryGetObjectField(Field.Key, TargetChildObject) && TargetChildObject && (*TargetChildObject).IsValid())
				{
					TargetChild = *TargetChildObject;
				}
				else
				{
					TargetChild = MakeShared<FJsonObject>();
					Target->SetObjectField(Field.Key, TargetChild);
				}
				DeepMergeJsonObject(*PatchChildObject, TargetChild);
			}
			else
			{
				Target->SetField(Field.Key, Field.Value);
			}
		}
	}
}

void UUmgThemeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActiveThemeAssetPath = GetDefaultThemeAssetPath();
	ReloadThemeCache();
	UE_LOG(LogUmgTheme, Log, TEXT("UmgThemeSubsystem initialized. Active theme: %s"), *ActiveThemeAssetPath);
}

void UUmgThemeSubsystem::Deinitialize()
{
	CachedTheme.Reset();
	Super::Deinitialize();
}

FString UUmgThemeSubsystem::GetDefaultThemeAssetPath() const
{
	return TEXT("/Game/UI/theme");
}

FString UUmgThemeSubsystem::ResolveThemeDiskPath(const FString& OptionalAssetPath) const
{
	const FString AssetPath = OptionalAssetPath.IsEmpty() ? ActiveThemeAssetPath : OptionalAssetPath;
	const FString PackagePath = AssetPath.Contains(TEXT(".")) ? FPaths::GetPath(AssetPath) : AssetPath;
	return FPackageName::LongPackageNameToFilename(PackagePath, TEXT(".json"));
}

bool UUmgThemeSubsystem::LoadThemeFromDisk(const FString& DiskPath)
{
	FString FileContent;
	if (!FPaths::FileExists(DiskPath) || !FFileHelper::LoadFileToString(FileContent, *DiskPath))
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UmgMcp")))
		{
			const FString DefaultPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/theme_default.json"));
			if (FFileHelper::LoadFileToString(FileContent, *DefaultPath))
			{
				UE_LOG(LogUmgTheme, Log, TEXT("Theme file missing at '%s'; using plugin default."), *DiskPath);
			}
			else
			{
				UE_LOG(LogUmgTheme, Warning, TEXT("No theme file at '%s' and no plugin default."), *DiskPath);
				CachedTheme = MakeShared<FJsonObject>();
				return false;
			}
		}
		else
		{
			CachedTheme = MakeShared<FJsonObject>();
			return false;
		}
	}

	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		UE_LOG(LogUmgTheme, Error, TEXT("Failed to parse theme JSON: %s"), *DiskPath);
		CachedTheme = MakeShared<FJsonObject>();
		return false;
	}

	CachedTheme = Parsed;
	return true;
}

bool UUmgThemeSubsystem::ReloadThemeCache(const FString& OptionalAssetPath)
{
	if (!OptionalAssetPath.IsEmpty())
	{
		ActiveThemeAssetPath = OptionalAssetPath.Contains(TEXT("."))
			? FPaths::GetPath(OptionalAssetPath)
			: OptionalAssetPath;
	}

	const FString DiskPath = ResolveThemeDiskPath(OptionalAssetPath);
	return LoadThemeFromDisk(DiskPath);
}

FString UUmgThemeSubsystem::GetThemeJsonString() const
{
	if (!CachedTheme.IsValid())
	{
		return TEXT("{}");
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(CachedTheme.ToSharedRef(), Writer);
	return Out;
}

TSharedPtr<FJsonValue> UUmgThemeSubsystem::NavigateTokenPath(const FString& DotPath) const
{
	if (!CachedTheme.IsValid() || DotPath.IsEmpty())
	{
		return nullptr;
	}

	TArray<FString> Segments;
	DotPath.ParseIntoArray(Segments, TEXT("."), true);
	const TSharedPtr<FJsonObject>* CurrentObject = &CachedTheme;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		if (!CurrentObject || !(*CurrentObject).IsValid())
		{
			return nullptr;
		}

		if (Index == Segments.Num() - 1)
		{
			return (*CurrentObject)->TryGetField(Segments[Index]);
		}

		const TSharedPtr<FJsonObject>* NextObject = nullptr;
		if (!(*CurrentObject)->TryGetObjectField(Segments[Index], NextObject))
		{
			return nullptr;
		}
		CurrentObject = NextObject;
	}

	return nullptr;
}

FString UUmgThemeSubsystem::ResolveToken(const FString& TokenRef) const
{
	if (const TSharedPtr<FJsonValue> ResolvedValue = ResolveTokenValue(TokenRef))
	{
		return UmgThemeInternal::JsonValueToResolveString(ResolvedValue);
	}

	FString Ref = TokenRef;
	Ref.TrimStartAndEndInline();
	if (Ref.StartsWith(TEXT("@")))
	{
		Ref = Ref.Mid(1);
	}
	if (!Ref.IsEmpty())
	{
		UE_LOG(LogUmgTheme, Warning, TEXT("Unresolved theme token '@%s'"), *Ref);
	}
	return FString();
}

TSharedPtr<FJsonValue> UUmgThemeSubsystem::ResolveTokenValue(const FString& TokenRef) const
{
	FString Ref = TokenRef;
	Ref.TrimStartAndEndInline();
	if (Ref.StartsWith(TEXT("@")))
	{
		Ref = Ref.Mid(1);
	}

	if (Ref.IsEmpty())
	{
		return nullptr;
	}

	if (TSharedPtr<FJsonValue> Found = NavigateTokenPath(Ref))
	{
		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Found.ToSharedRef(), TEXT(""), Writer);

		TSharedPtr<FJsonValue> Cloned;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
		if (FJsonSerializer::Deserialize(Reader, Cloned) && Cloned.IsValid())
		{
			return Cloned;
		}
	}

	UE_LOG(LogUmgTheme, Warning, TEXT("Unresolved theme token '@%s'"), *Ref);
	return nullptr;
}

void UUmgThemeSubsystem::ProcessJsonValue(TSharedPtr<FJsonValue>& Value, int32 Depth) const
{
	if (!Value.IsValid())
	{
		return;
	}

	if (Depth > MaxTokenProcessDepth)
	{
		UE_LOG(LogUmgTheme, Error, TEXT("ProcessJsonValue: exceeded max depth (%d). Aborting token resolution for this branch."), MaxTokenProcessDepth);
		return;
	}

	if (Value->Type == EJson::String)
	{
		const FString Str = Value->AsString();
		if (Str.StartsWith(TEXT("@")))
		{
			if (const TSharedPtr<FJsonValue> ResolvedValue = ResolveTokenValue(Str))
			{
				Value = ResolvedValue;
				// Resolved value may itself contain nested @ tokens or objects.
				ProcessJsonValue(Value, Depth + 1);
			}
			else
			{
				UE_LOG(LogUmgTheme, Warning, TEXT("ProcessJsonValue: failed to resolve token '%s'"), *Str);
			}
		}
		return;
	}

	if (Value->Type == EJson::Object)
	{
		TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (Obj.IsValid())
		{
			ProcessJsonTokens(Obj, Depth + 1);
			Value = MakeShared<FJsonValueObject>(Obj);
		}
		return;
	}

	if (Value->Type == EJson::Array)
	{
		TArray<TSharedPtr<FJsonValue>> Items = Value->AsArray();
		for (TSharedPtr<FJsonValue>& Item : Items)
		{
			ProcessJsonValue(Item, Depth + 1);
		}
		Value = MakeShared<FJsonValueArray>(Items);
	}
}

void UUmgThemeSubsystem::ProcessJsonTokens(TSharedPtr<FJsonObject>& InOutJson, int32 Depth) const
{
	if (!InOutJson.IsValid())
	{
		return;
	}

	if (Depth > MaxTokenProcessDepth)
	{
		UE_LOG(LogUmgTheme, Error, TEXT("ProcessJsonTokens: exceeded max depth (%d). Aborting token resolution."), MaxTokenProcessDepth);
		return;
	}

	// Never mutate Values while iterating it — SetField can rehash and invalidate iterators.
	TArray<FString> FieldKeys;
	FieldKeys.Reserve(InOutJson->Values.Num());
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : InOutJson->Values)
	{
		FieldKeys.Add(Pair.Key);
	}

	for (const FString& FieldKey : FieldKeys)
	{
		TSharedPtr<FJsonValue> FieldValue = InOutJson->TryGetField(FieldKey);
		if (!FieldValue.IsValid())
		{
			UE_LOG(LogUmgTheme, Verbose, TEXT("ProcessJsonTokens: missing field '%s' during token pass (skipped)."), *FieldKey);
			continue;
		}

		ProcessJsonValue(FieldValue, Depth + 1);
		InOutJson->SetField(FieldKey, FieldValue);
	}
}

bool UUmgThemeSubsystem::ApplyThemePatch(const TSharedPtr<FJsonObject>& PatchObject, const FString& OptionalAssetPath)
{
	if (!PatchObject.IsValid())
	{
		return false;
	}

	if (!OptionalAssetPath.IsEmpty())
	{
		ActiveThemeAssetPath = OptionalAssetPath.Contains(TEXT("."))
			? FPaths::GetPath(OptionalAssetPath)
			: OptionalAssetPath;
	}

	if (!CachedTheme.IsValid())
	{
		CachedTheme = MakeShared<FJsonObject>();
	}

	UmgThemeInternal::DeepMergeJsonObject(PatchObject, CachedTheme);

	const FString DiskPath = ResolveThemeDiskPath(OptionalAssetPath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(DiskPath), true);

	FString OutJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	FJsonSerializer::Serialize(CachedTheme.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(OutJson, *DiskPath))
	{
		UE_LOG(LogUmgTheme, Error, TEXT("Failed to write theme file: %s"), *DiskPath);
		return false;
	}

	UE_LOG(LogUmgTheme, Log, TEXT("Theme patch applied and saved to %s"), *DiskPath);
	return ReloadThemeCache(OptionalAssetPath);
}
