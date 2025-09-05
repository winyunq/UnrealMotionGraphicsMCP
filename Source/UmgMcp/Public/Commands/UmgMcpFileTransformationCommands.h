// UmgMcpFileTransformationCommands.h
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UmgFileTransformation.h" // Include our file transformation utility

class FUmgMcpFileTransformationCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
};