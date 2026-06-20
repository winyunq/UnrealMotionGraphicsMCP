// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Widget/UmgMcpPropertyJsonUtils.h"

#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"

namespace UmgMcpPropertyJsonUtilsInternal
{
	static constexpr int32 MaxSerializeDepth = 32;

	static bool ShouldSkipSlotProperty(FName PropertyName)
	{
		return PropertyName == TEXT("Content") || PropertyName == TEXT("Parent");
	}

	static bool ShouldExportAsObjectPath(UObject* Object)
	{
		if (!Object)
		{
			return false;
		}

		return Object->IsA<UWidget>() || Object->IsA<UPanelSlot>();
	}
}

TSharedPtr<FJsonObject> FUmgMcpPropertyJsonUtils::SerializePanelSlotProperties(UPanelSlot* SlotObject, bool bOnlyNonDefault)
{
	using namespace UmgMcpPropertyJsonUtilsInternal;

	if (!SlotObject)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> SlotPropertiesJson = MakeShared<FJsonObject>();
	UObject* DefaultSlotObject = SlotObject->GetClass()->GetDefaultObject();

	for (TFieldIterator<FProperty> SlotPropIt(SlotObject->GetClass()); SlotPropIt; ++SlotPropIt)
	{
		FProperty* SlotProperty = *SlotPropIt;
		if (ShouldSkipSlotProperty(SlotProperty->GetFName()))
		{
			continue;
		}

		if (!SlotProperty->HasAnyPropertyFlags(CPF_Edit) || SlotProperty->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		void* SlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(SlotObject);
		if (bOnlyNonDefault)
		{
			void* DefaultSlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(DefaultSlotObject);
			if (SlotProperty->Identical(SlotValuePtr, DefaultSlotValuePtr))
			{
				continue;
			}
		}

		if (TSharedPtr<FJsonValue> SlotPropertyJsonValue = PropertyToJsonValue(SlotProperty, SlotValuePtr))
		{
			SlotPropertiesJson->SetField(SlotProperty->GetName(), SlotPropertyJsonValue);
		}
	}

	return SlotPropertiesJson->Values.Num() > 0 ? SlotPropertiesJson : nullptr;
}

TSharedPtr<FJsonValue> FUmgMcpPropertyJsonUtils::PropertyToJsonValue(FProperty* Property, const void* ValuePtr, int32 Depth)
{
	using namespace UmgMcpPropertyJsonUtilsInternal;

	if (!Property || !ValuePtr)
	{
		return nullptr;
	}

	if (Depth > MaxSerializeDepth)
	{
		return MakeShared<FJsonValueString>(TEXT("<max_depth>"));
	}

	if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(ValuePtr);
		if (!ObjectValue)
		{
			return MakeShared<FJsonValueNull>();
		}

		if (UPanelSlot* SlotObject = Cast<UPanelSlot>(ObjectValue))
		{
			if (TSharedPtr<FJsonObject> SlotJson = SerializePanelSlotProperties(SlotObject, /*bOnlyNonDefault*/ false))
			{
				return MakeShared<FJsonValueObject>(SlotJson);
			}
			return MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
		}

		if (ShouldExportAsObjectPath(ObjectValue))
		{
			return MakeShared<FJsonValueString>(ObjectValue->GetPathName());
		}

		return MakeShared<FJsonValueString>(ObjectValue->GetPathName());
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		TSharedPtr<FJsonObject> StructJson = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> FieldIt(StructProperty->Struct); FieldIt; ++FieldIt)
		{
			FProperty* FieldProperty = *FieldIt;
			const void* FieldValuePtr = FieldProperty->ContainerPtrToValuePtr<void>(ValuePtr);
			if (TSharedPtr<FJsonValue> FieldJson = PropertyToJsonValue(FieldProperty, FieldValuePtr, Depth + 1))
			{
				StructJson->SetField(FieldProperty->GetName(), FieldJson);
			}
		}
		return MakeShared<FJsonValueObject>(StructJson);
	}

	return FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr);
}

bool FUmgMcpPropertyJsonUtils::TryWriteSlotPropertiesField(TSharedPtr<FJsonObject>& OutJson, const FString& FieldName, UWidget* Widget)
{
	if (!OutJson.IsValid() || !Widget || !Widget->Slot)
	{
		return false;
	}

	if (TSharedPtr<FJsonObject> SlotJson = SerializePanelSlotProperties(Widget->Slot))
	{
		OutJson->SetObjectField(FieldName, SlotJson);
		return true;
	}

	return false;
}
