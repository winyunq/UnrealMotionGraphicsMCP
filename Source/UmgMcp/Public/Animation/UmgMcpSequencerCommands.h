#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"

/**
 * @brief Handles all MCP commands for querying and manipulating UMG Animations (Sequencer).
 * Updated for v2.0 Read-Write-Attention API.
 */
class FUmgMcpSequencerCommands
{
public:
    FUmgMcpSequencerCommands();
    ~FUmgMcpSequencerCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params);

private:
    // Attention (Context)
    TSharedPtr<FJsonObject> SetAnimationScope(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> SetWidgetScope(const TSharedPtr<FJsonObject>& Params);

    // Read (Sensing)
    TSharedPtr<FJsonObject> GetAllAnimations(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetAnimationKeyframes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetAnimatedWidgets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetAnimationFullData(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetWidgetAnimationData(const TSharedPtr<FJsonObject>& Params);

    // Write (Action)
    TSharedPtr<FJsonObject> CreateAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> DeleteAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> SetPropertyKeys(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> RemovePropertyTrack(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> RemoveKeys(const TSharedPtr<FJsonObject>& Params);
};
