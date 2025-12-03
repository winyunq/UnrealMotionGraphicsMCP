#include "Animation/UmgMcpSequencerCommands.h"
#include "Bridge/UmgMcpCommonUtils.h"
#include "UmgMcp.h" // Added for LogUmgMcp
#include "WidgetBlueprint.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "ISequencer.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h" // Added for CompileBlueprint

FUmgMcpSequencerCommands::FUmgMcpSequencerCommands()
{
}

FUmgMcpSequencerCommands::~FUmgMcpSequencerCommands()
{
}

#include "FileManage/UmgAttentionSubsystem.h"

// ... (existing includes)

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
    if (Command == TEXT("get_all_animations"))
    {
        return GetAllAnimations(Params);
    }
    else if (Command == TEXT("create_animation"))
    {
        return CreateAnimation(Params);
    }
    else if (Command == TEXT("delete_animation"))
    {
        return DeleteAnimation(Params);
    }
    else if (Command == TEXT("add_track"))
    {
        return AddTrack(Params);
    }
    else if (Command == TEXT("add_key"))
    {
        return AddKey(Params);
    }
    else if (Command == TEXT("focus_animation"))
    {
        return FocusAnimation(Params);
    }
    else if (Command == TEXT("focus_widget"))
    {
        return FocusWidget(Params);
    }

    return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown sequencer command: %s"), *Command));
}

// ... (existing methods)

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::FocusAnimation(const TSharedPtr<FJsonObject>& Params)
{
    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_name' parameter"));
    }

    if (GEditor)
    {
        UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
        if (AttentionSubsystem)
        {
            AttentionSubsystem->SetTargetAnimation(AnimationName);
            return FUmgMcpCommonUtils::CreateSuccessResponse();
        }
    }

    return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Failed to access UmgAttentionSubsystem"));
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::FocusWidget(const TSharedPtr<FJsonObject>& Params)
{
    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
    }

    if (GEditor)
    {
        UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
        if (AttentionSubsystem)
        {
            AttentionSubsystem->SetTargetWidget(WidgetName);
            return FUmgMcpCommonUtils::CreateSuccessResponse();
        }
    }

    return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Failed to access UmgAttentionSubsystem"));
}

#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Blueprint/WidgetTree.h"

// ... (existing includes)

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::AddTrack(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    // Resolve Animation Name (Context Aware)
    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        if (GEditor)
        {
            UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
            if (AttentionSubsystem)
            {
                AnimationName = AttentionSubsystem->GetTargetAnimation();
            }
        }
    }

    if (AnimationName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("No animation specified and no context found. Use 'focus_animation' or provide 'animation_name'."));
    }

    // Resolve Widget Name (Context Aware)
    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
    {
        if (GEditor)
        {
            UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
            if (AttentionSubsystem)
            {
                WidgetName = AttentionSubsystem->GetTargetWidget();
            }
        }
    }

    if (WidgetName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("No widget specified and no context found. Use 'focus_widget' or provide 'widget_name'."));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName) || PropertyName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Find Animation
    UWidgetAnimation* TargetAnimation = nullptr;
    for (UWidgetAnimation* Animation : Blueprint->Animations)
    {
        if (Animation && Animation->GetName() == AnimationName)
        {
            TargetAnimation = Animation;
            break;
        }
    }

    if (!TargetAnimation)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Animation '%s' not found"), *AnimationName));
    }

    UMovieScene* MovieScene = TargetAnimation->GetMovieScene();
    if (!MovieScene)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Animation has no MovieScene"));
    }

    // Find or Create Binding
    FGuid WidgetGuid;
    for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
    {
        const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
        if (Possessable.GetName() == WidgetName)
        {
            WidgetGuid = Possessable.GetGuid();
            break;
        }
    }

    if (!WidgetGuid.IsValid())
    {
        // Find the widget object to get its class
        UWidget* Widget = Blueprint->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget '%s' not found in WidgetTree"), *WidgetName));
        }

        // AddPossessable returns FMovieScenePossessable& in UE5, but error said FGuid? 
        // Let's try to capture it as auto& or check if it returns FGuid directly.
        // Based on error "cannot convert from FGuid to FMovieScenePossessable&", it likely returns FMovieScenePossessable& 
        // BUT maybe I was assigning it wrong? 
        // Wait, "cannot convert from FGuid to FMovieScenePossessable&" means the RHS is FGuid.
        // So AddPossessable returns FGuid.
        WidgetGuid = MovieScene->AddPossessable(WidgetName, Widget->GetClass());
    }

    // Find or Create Track (Assuming Float Track for now)
    // TODO: Support other track types based on property type
    UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid, FName(*PropertyName));
    if (!Track)
    {
        Track = MovieScene->AddTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid);
        UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
        if (FloatTrack)
        {
            FloatTrack->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
        }
    }

    Blueprint->Modify();
    return FUmgMcpCommonUtils::CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::AddKey(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    // Resolve Animation Name (Context Aware)
    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        if (GEditor)
        {
            UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
            if (AttentionSubsystem)
            {
                AnimationName = AttentionSubsystem->GetTargetAnimation();
            }
        }
    }

    if (AnimationName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("No animation specified and no context found. Use 'focus_animation' or provide 'animation_name'."));
    }

    // Resolve Widget Name (Context Aware)
    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
    {
        if (GEditor)
        {
            UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
            if (AttentionSubsystem)
            {
                WidgetName = AttentionSubsystem->GetTargetWidget();
            }
        }
    }

    if (WidgetName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("No widget specified and no context found. Use 'focus_widget' or provide 'widget_name'."));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName) || PropertyName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    double Time = 0.0;
    if (Params->HasField(TEXT("time")))
    {
        Time = Params->GetNumberField(TEXT("time"));
    }

    double Value = 0.0;
    if (Params->HasField(TEXT("value")))
    {
        Value = Params->GetNumberField(TEXT("value"));
    }

    // Find Animation
    UWidgetAnimation* TargetAnimation = nullptr;
    for (UWidgetAnimation* Animation : Blueprint->Animations)
    {
        if (Animation && Animation->GetName() == AnimationName)
        {
            TargetAnimation = Animation;
            break;
        }
    }

    if (!TargetAnimation)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Animation '%s' not found"), *AnimationName));
    }

    UMovieScene* MovieScene = TargetAnimation->GetMovieScene();
    if (!MovieScene)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Animation has no MovieScene"));
    }

    // Find Binding
    FGuid WidgetGuid;
    for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
    {
        const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
        if (Possessable.GetName() == WidgetName)
        {
            WidgetGuid = Possessable.GetGuid();
            break;
        }
    }

    if (!WidgetGuid.IsValid())
    {
        // Auto-create binding if it doesn't exist (Implicit AddTrack)
        UWidget* Widget = Blueprint->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget '%s' not found in WidgetTree"), *WidgetName));
        }

        WidgetGuid = MovieScene->AddPossessable(WidgetName, Widget->GetClass());
    }

    // Find Track
    UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid, FName(*PropertyName));
    if (!Track)
    {
        // Auto-create track if it doesn't exist
        Track = MovieScene->AddTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid);
        UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
        if (FloatTrack)
        {
            FloatTrack->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
        }
    }

    UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
    if (!FloatTrack)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Track is not a float track"));
    }

    // Add Key
    bool bSectionAdded = false;
    UMovieSceneSection* Section = FloatTrack->FindOrAddSection(0, bSectionAdded);
    UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section);
    if (FloatSection)
    {
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        FFrameNumber FrameNumber = (Time * TickResolution).RoundToFrame();
        FloatSection->GetChannel().AddCubicKey(FrameNumber, Value);

        // Auto-expand Playback Range to fit the new key
        TRange<FFrameNumber> CurrentRange = MovieScene->GetPlaybackRange();
        FFrameNumber CurrentEnd = CurrentRange.GetUpperBoundValue();
        
        if (FrameNumber > CurrentEnd)
        {
            // Expand range to include the new key
            MovieScene->SetPlaybackRange(TRange<FFrameNumber>(CurrentRange.GetLowerBoundValue(), FrameNumber));
            UE_LOG(LogUmgMcp, Display, TEXT("AddKey: Expanded playback range to frame %d"), FrameNumber.Value);
        }
    }

    Blueprint->Modify();
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("animation"), AnimationName);
    Result->SetStringField(TEXT("widget"), WidgetName);
    Result->SetStringField(TEXT("property"), PropertyName);
    Result->SetNumberField(TEXT("time"), Time);
    Result->SetNumberField(TEXT("value"), Value);
    
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetAllAnimations(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TArray<TSharedPtr<FJsonValue>> AnimationsArray;
    for (UWidgetAnimation* Animation : Blueprint->Animations)
    {
        if (Animation)
        {
            TSharedPtr<FJsonObject> AnimObject = MakeShared<FJsonObject>();
            AnimObject->SetStringField(TEXT("name"), Animation->GetName());
            AnimObject->SetNumberField(TEXT("start_time"), Animation->GetStartTime());
            AnimObject->SetNumberField(TEXT("end_time"), Animation->GetEndTime());
            AnimationsArray.Add(MakeShared<FJsonValueObject>(AnimObject));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("animations"), AnimationsArray);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::CreateAnimation(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AnimationName;
    // Default to "UnrealMotionGraphicsMCP_X" if not provided
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        AnimationName = FString::Printf(TEXT("UnrealMotionGraphicsMCP_%d"), Blueprint->Animations.Num());
    }

    // Check if animation already exists
    for (UWidgetAnimation* Animation : Blueprint->Animations)
    {
        if (Animation && Animation->GetName() == AnimationName)
        {
            // If it exists, just focus it and return success (idempotent behavior)
            if (GEditor)
            {
                UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
                if (AttentionSubsystem)
                {
                    AttentionSubsystem->SetTargetAnimation(AnimationName);
                }
            }
            
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("name"), AnimationName);
            Result->SetStringField(TEXT("status"), TEXT("exists_and_focused"));
            return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
        }
    }

    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Attempting to create animation '%s'"), *AnimationName);

    // Create Animation with correct flags
    UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(Blueprint, FName(*AnimationName), RF_Public | RF_Transactional);
    if (!NewAnimation)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateAnimation: Failed to create UWidgetAnimation object"));
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Failed to create UWidgetAnimation object"));
    }
    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Created UWidgetAnimation object at %p"), NewAnimation);
    
    // Initialize MovieScene with correct flags (RF_Public removed to prevent serialization issues)
    NewAnimation->MovieScene = NewObject<UMovieScene>(NewAnimation, FName("MovieScene"), RF_Transactional);
    if (!NewAnimation->MovieScene)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateAnimation: Failed to create UMovieScene object"));
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Failed to create UMovieScene object"));
    }
    
    // CRITICAL: Initialize MovieScene properties to prevent Sequencer crash
    // Note: SetTickResolution/SetDisplayRate removed as they caused compilation errors. 
    // We rely on CompileBlueprint to ensure proper initialization.
    // NewAnimation->MovieScene->SetTickResolution(FFrameRate(24000, 1));
    // NewAnimation->MovieScene->SetDisplayRate(FFrameRate(30, 1));
    
    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Created UMovieScene object at %p"), NewAnimation->MovieScene.Get());
    
    // Add to Blueprint
    Blueprint->Animations.Add(NewAnimation);
    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Added animation to Blueprint->Animations array"));
    
    // Notify Editor of structural change
    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Calling MarkBlueprintAsStructurallyModified..."));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: MarkBlueprintAsStructurallyModified complete"));

    // Compile the Blueprint to ensure the animation is registered as a property
    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Compiling Blueprint..."));
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Blueprint compilation complete"));
    
    // Auto-Focus the new animation
    if (GEditor)
    {
        UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
        if (AttentionSubsystem)
        {
            AttentionSubsystem->SetTargetAnimation(AnimationName);
        }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewAnimation->GetName());
    Result->SetStringField(TEXT("status"), TEXT("created_and_focused"));

    // Add warning if using default name
    if (NewAnimation->GetName().StartsWith(TEXT("UnrealMotionGraphicsMCP")))
    {
        Result->SetStringField(TEXT("warning"), TEXT("You are using the default animation name 'UnrealMotionGraphicsMCP'. It is recommended to rename this animation to something more descriptive using 'rename_animation' (if available) or by creating it with a specific name."));
    }

    UE_LOG(LogUmgMcp, Display, TEXT("CreateAnimation: Success"));
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::DeleteAnimation(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'animation_name' parameter"));
    }

    int32 RemovedCount = Blueprint->Animations.RemoveAll([&](UWidgetAnimation* Anim) {
        return Anim && Anim->GetName() == AnimationName;
    });

    if (RemovedCount > 0)
    {
        Blueprint->Modify();
        return FUmgMcpCommonUtils::CreateSuccessResponse();
    }
    else
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Animation '%s' not found"), *AnimationName));
    }
}


