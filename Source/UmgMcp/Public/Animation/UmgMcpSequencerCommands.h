#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"

/**
 * @brief Handles all MCP commands for querying and manipulating UMG Animations (Sequencer).
 */
class FUmgMcpSequencerCommands
{
public:
    FUmgMcpSequencerCommands();
    ~FUmgMcpSequencerCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> GetAllAnimations(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> CreateAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> DeleteAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> AddTrack(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> AddKey(const TSharedPtr<FJsonObject>& Params);

    // Context Handlers
    TSharedPtr<FJsonObject> FocusAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> FocusWidget(const TSharedPtr<FJsonObject>& Params);
};
