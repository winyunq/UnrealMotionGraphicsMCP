// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"

class UWidgetBlueprint;
class UMovieScene;

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
    // Helpers
    TSharedPtr<FJsonObject> ResolveAnimationContext(const TSharedPtr<FJsonObject>& Params, UWidgetBlueprint*& OutBlueprint, UWidgetAnimation*& OutAnimation, FString& OutError) const;
    bool ResolveWidgetName(const TSharedPtr<FJsonObject>& Params, FString& OutWidgetName) const;

    // Attention (Context)
    TSharedPtr<FJsonObject> SetAnimationScope(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> SetWidgetScope(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> SetAnimationTarget(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> SetWidgetTarget(const TSharedPtr<FJsonObject>& Params);

    // Read (Sensing)
    TSharedPtr<FJsonObject> GetAllAnimations(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetAnimationKeyframes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetAnimatedWidgets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetAnimationFullData(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetWidgetAnimationData(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetWidgetPropertyTimeline(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetTimeSliceProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetAnimationOverview(const TSharedPtr<FJsonObject>& Params);

    // Write (Action)
    TSharedPtr<FJsonObject> CreateAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> DeleteAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> SetPropertyKeys(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> RemovePropertyTrack(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> RemoveKeys(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> AppendWidgetTracks(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> AppendTimeSlice(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> DeleteWidgetKeys(const TSharedPtr<FJsonObject>& Params);
    
    // Level 2 API
    TSharedPtr<FJsonObject> SetAnimationData(const TSharedPtr<FJsonObject>& Params);
};
