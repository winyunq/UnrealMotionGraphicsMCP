// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

class FProperty;
class UPanelSlot;
class UWidget;

/**
 * Safe JSON export helpers for UMG widget/slot properties.
 * Prevents UObject cycles (Widget -> Slot -> Content -> Widget) from blowing the stack
 * inside FJsonObjectConverter::UPropertyToJsonValue.
 */
class UMGMCP_API FUmgMcpPropertyJsonUtils
{
public:
	/** Export editable slot fields, always skipping Content/Parent back-links. */
	static TSharedPtr<FJsonObject> SerializePanelSlotProperties(UPanelSlot* SlotObject, bool bOnlyNonDefault = true);

	/** Drop-in replacement for FJsonObjectConverter::UPropertyToJsonValue on UMG objects. */
	static TSharedPtr<FJsonValue> PropertyToJsonValue(FProperty* Property, const void* ValuePtr, int32 Depth = 0);

	/** Query helper: write slot layout JSON for a widget (Position/Size aliases handled elsewhere). */
	static bool TryWriteSlotPropertiesField(TSharedPtr<FJsonObject>& OutJson, const FString& FieldName, UWidget* Widget);
};
