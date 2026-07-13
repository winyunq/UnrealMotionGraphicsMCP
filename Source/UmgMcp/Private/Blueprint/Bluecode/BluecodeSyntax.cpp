// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Blueprint/Bluecode/BluecodeSyntax.h"

int32 FBluecodeSyntax::FindTopLevelAssignmentOperator(const FString& Statement)
{
	TCHAR Quote = 0;
	int32 Depth = 0;
	for (int32 Index = 0; Index < Statement.Len(); ++Index)
	{
		const TCHAR Ch = Statement[Index];
		if (Quote != 0)
		{
			if (Ch == Quote && (Index == 0 || Statement[Index - 1] != TEXT('\\')))
			{
				Quote = 0;
			}
			continue;
		}
		if (Ch == TEXT('"') || Ch == TEXT('\''))
		{
			Quote = Ch;
			continue;
		}
		if (Ch == TEXT('(') || Ch == TEXT('[') || Ch == TEXT('{'))
		{
			++Depth;
			continue;
		}
		if (Ch == TEXT(')') || Ch == TEXT(']') || Ch == TEXT('}'))
		{
			Depth = FMath::Max(0, Depth - 1);
			continue;
		}
		if (Depth == 0 && Ch == TEXT('='))
		{
			const TCHAR Prev = Index > 0 ? Statement[Index - 1] : 0;
			const TCHAR Next = Index + 1 < Statement.Len() ? Statement[Index + 1] : 0;
			if (Prev != TEXT('=') && Prev != TEXT('!') && Prev != TEXT('<') && Prev != TEXT('>') && Next != TEXT('='))
			{
				return Index;
			}
		}
	}
	return INDEX_NONE;
}

bool FBluecodeSyntax::IsIdentifier(const FString& Value)
{
	FString Text = Value;
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty() || !(FChar::IsAlpha(Text[0]) || Text[0] == TEXT('_')))
	{
		return false;
	}
	for (int32 Index = 1; Index < Text.Len(); ++Index)
	{
		if (!(FChar::IsAlnum(Text[Index]) || Text[Index] == TEXT('_')))
		{
			return false;
		}
	}
	return true;
}
