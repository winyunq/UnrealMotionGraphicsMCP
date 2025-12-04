#include "Animation/UmgMcpSequencerCommands.h"
#include "Bridge/UmgMcpCommonUtils.h"
#include "UmgMcp.h"
#include "WidgetBlueprint.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "ISequencer.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Blueprint/WidgetTree.h"

// Sequencer Includes
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneChannelProxy.h"

// Define a log category for Sequencer commands
DEFINE_LOG_CATEGORY_STATIC(LogUmgSequencer, Log, All);

FUmgMcpSequencerCommands::FUmgMcpSequencerCommands()
{
}

FUmgMcpSequencerCommands::~FUmgMcpSequencerCommands()
{
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
    // Attention
    if (Command == TEXT("set_animation_scope")) return SetAnimationScope(Params);
    if (Command == TEXT("set_widget_scope")) return SetWidgetScope(Params);

    // Read
    if (Command == TEXT("get_all_animations")) return GetAllAnimations(Params);
    if (Command == TEXT("get_animation_keyframes")) return GetAnimationKeyframes(Params);
    if (Command == TEXT("get_animated_widgets")) return GetAnimatedWidgets(Params);
    if (Command == TEXT("get_animation_full_data")) return GetAnimationFullData(Params);
    if (Command == TEXT("get_widget_animation_data")) return GetWidgetAnimationData(Params);

    // Write
    if (Command == TEXT("create_animation")) return CreateAnimation(Params);
    if (Command == TEXT("delete_animation")) return DeleteAnimation(Params);
    if (Command == TEXT("set_property_keys")) return SetPropertyKeys(Params);
    if (Command == TEXT("remove_property_track")) return RemovePropertyTrack(Params);
    if (Command == TEXT("remove_keys")) return RemoveKeys(Params);

    return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown sequencer command: %s"), *Command));
}

// =============================================================================
//  Attention (Context)
// =============================================================================

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::SetAnimationScope(const TSharedPtr<FJsonObject>& Params)
{
    // POLICY CHANGE: "Select = Ensure Exists"
    // As per user request, selecting an animation should ensure it exists (create if missing) and then focus it.
    // CreateAnimation already implements "Find or Create + Focus", so we simply delegate to it.
    return CreateAnimation(Params);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::SetWidgetScope(const TSharedPtr<FJsonObject>& Params)
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

// =============================================================================
//  Read (Sensing)
// =============================================================================

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetAllAnimations(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("GetAllAnimations: Called."));

    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) 
    {
        UE_LOG(LogUmgSequencer, Error, TEXT("GetAllAnimations: Failed to get blueprint. %s"), *ErrorMessage);
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

    UE_LOG(LogUmgSequencer, Log, TEXT("GetAllAnimations: Found %d animations."), AnimationsArray.Num());

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("animations"), AnimationsArray);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetAnimationKeyframes(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("GetAnimationKeyframes: Called."));

    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);

    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        // Try context
        if (GEditor)
        {
            if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
                AnimationName = Sub->GetTargetAnimation();
        }
    }

    if (AnimationName.IsEmpty()) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_name'"));

    UWidgetAnimation* TargetAnimation = nullptr;
    for (UWidgetAnimation* Anim : Blueprint->Animations)
    {
        if (Anim && Anim->GetName() == AnimationName)
        {
            TargetAnimation = Anim;
            break;
        }
    }
    if (!TargetAnimation) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Animation not found"));

    UMovieScene* MovieScene = TargetAnimation->GetMovieScene();
    if (!MovieScene) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("MovieScene is null"));

    TArray<TSharedPtr<FJsonValue>> TracksArray;

    // Iterate over bindings to find widgets, then tracks
    for (const FWidgetAnimationBinding& Binding : TargetAnimation->AnimationBindings)
    {
        FGuid ObjectGuid = Binding.AnimationGuid;
        FString WidgetName = Binding.WidgetName.ToString();

        // Find tracks for this object
        for (const UMovieSceneTrack* Track : MovieScene->GetTracks())
        {
             // Check if this track is bound to this object
             // Note: MovieScene tracks are bound to GUIDs. We need to check if this track relates to our ObjectGuid.
             // However, GetTracks() returns all tracks. We need to filter.
             // Actually, usually we ask the MovieScene for tracks bound to a GUID.
        }
        
        // Correct approach: Iterate all bindings, then ask MovieScene for tracks for that binding
        for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneFloatTrack::StaticClass(), ObjectGuid))
        {
             if (const UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track))
             {
                 TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
                 TrackObj->SetStringField(TEXT("widget_name"), WidgetName);
                 TrackObj->SetStringField(TEXT("property_name"), FloatTrack->GetPropertyName().ToString());

                 TArray<TSharedPtr<FJsonValue>> KeysArray;
                 
                 // Assuming section 0 for simplicity
                 if (FloatTrack->GetAllSections().Num() > 0)
                 {
                     if (const UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>(FloatTrack->GetAllSections()[0]))
                     {
                         TArrayView<const FFrameNumber> Times = Section->GetChannel().GetData().GetTimes();
                         TArrayView<const FMovieSceneFloatValue> Values = Section->GetChannel().GetData().GetValues();
                         
                         FFrameRate TickResolution = MovieScene->GetTickResolution();

                         for (int32 i = 0; i < Times.Num(); ++i)
                         {
                             TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
                             double Time = (double)Times[i].Value / (double)TickResolution.Numerator * (double)TickResolution.Denominator; // Simplified conversion
                             KeyObj->SetNumberField(TEXT("time"), Time);
                             KeyObj->SetNumberField(TEXT("value"), Values[i].Value);
                             KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
                         }
                     }
                 }
                 TrackObj->SetArrayField(TEXT("keys"), KeysArray);
                 TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
             }
        }
    }

    UE_LOG(LogUmgSequencer, Log, TEXT("GetAnimationKeyframes: Found %d tracks for animation '%s'."), TracksArray.Num(), *AnimationName);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("tracks"), TracksArray);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetAnimatedWidgets(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("GetAnimatedWidgets: Called."));

    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);

    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
         if (GEditor)
        {
            if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
                AnimationName = Sub->GetTargetAnimation();
        }
    }
    if (AnimationName.IsEmpty()) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_name'"));

    UWidgetAnimation* TargetAnimation = nullptr;
    for (UWidgetAnimation* Anim : Blueprint->Animations)
    {
        if (Anim && Anim->GetName() == AnimationName)
        {
            TargetAnimation = Anim;
            break;
        }
    }
    if (!TargetAnimation) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Animation not found"));

    TArray<TSharedPtr<FJsonValue>> WidgetsArray;
    for (const FWidgetAnimationBinding& Binding : TargetAnimation->AnimationBindings)
    {
        TSharedPtr<FJsonObject> WidgetObj = MakeShared<FJsonObject>();
        WidgetObj->SetStringField(TEXT("widget_name"), Binding.WidgetName.ToString());
        WidgetObj->SetStringField(TEXT("guid"), Binding.AnimationGuid.ToString());
        WidgetObj->SetBoolField(TEXT("is_root"), Binding.bIsRootWidget);
        WidgetsArray.Add(MakeShared<FJsonValueObject>(WidgetObj));
    }

    UE_LOG(LogUmgSequencer, Log, TEXT("GetAnimatedWidgets: Found %d bound widgets for animation '%s'."), WidgetsArray.Num(), *AnimationName);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("widgets"), WidgetsArray);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetAnimationFullData(const TSharedPtr<FJsonObject>& Params)
{
    // Re-use GetAnimationKeyframes for now as it provides the bulk of the data
    return GetAnimationKeyframes(Params);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetWidgetAnimationData(const TSharedPtr<FJsonObject>& Params)
{
    // TODO: Filter GetAnimationKeyframes by widget
    return FUmgMcpCommonUtils::CreateSuccessResponse();
}

// =============================================================================
//  Write (Action)
// =============================================================================

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::CreateAnimation(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: Called."));

    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) 
    {
        UE_LOG(LogUmgSequencer, Error, TEXT("CreateAnimation: Failed to get target blueprint. Error: %s"), *ErrorMessage);
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        AnimationName = FString::Printf(TEXT("UnrealMotionGraphicsMCP_%d"), Blueprint->Animations.Num());
        UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: No name provided. Auto-generated name: %s"), *AnimationName);
    }
    else
    {
        UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: Request to create animation named: %s"), *AnimationName);
    }

    // Check existence
    for (UWidgetAnimation* Animation : Blueprint->Animations)
    {
        if (Animation && Animation->GetName() == AnimationName)
        {
            UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: Animation '%s' already exists. Setting focus."), *AnimationName);
            // Focus and return
            if (GEditor)
            {
                if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
                    Sub->SetTargetAnimation(AnimationName);
            }
            return FUmgMcpCommonUtils::CreateSuccessResponse();
        }
    }

    UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: Creating new UWidgetAnimation object..."));
    UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(Blueprint, FName(*AnimationName), RF_Public | RF_Transactional);
    NewAnimation->MovieScene = NewObject<UMovieScene>(NewAnimation, FName("MovieScene"), RF_Transactional);
    
    // Ensure MovieScene has a valid display name (though UMG usually uses the Animation name)
    // NewAnimation->MovieScene->SetDisplayLabel(AnimationName);

    Blueprint->Modify();
    Blueprint->Animations.Add(NewAnimation);
    
    // CRITICAL FIX: Assign a GUID to the new animation so it's recognized as a variable.
    // This prevents "Ensure condition failed: WidgetBP->WidgetVariableNameToGuidMap.Contains(Animation->GetFName())"
    FGuid NewAnimGuid = FGuid::NewGuid();
    Blueprint->WidgetVariableNameToGuidMap.Add(NewAnimation->GetFName(), NewAnimGuid);
    
    UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: Animation added to Blueprint with GUID %s. Notifying Editor..."), *NewAnimGuid.ToString());

    // Notify Editor
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    
    // Force a recompile to ensure the class generated includes the new animation property
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    if (GEditor)
    {
        if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            Sub->SetTargetAnimation(AnimationName);
    }

    UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: Successfully created animation '%s'."), *NewAnimation->GetName());

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewAnimation->GetName());
    
    // Add Debug Context
    FString ContextPath = Blueprint->GetPathName();
    FString ContextPtr = FString::Printf(TEXT("%p"), Blueprint);
    Result->SetStringField(TEXT("context_path"), ContextPath);
    Result->SetStringField(TEXT("context_ptr"), ContextPtr);
    
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::DeleteAnimation(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);

    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_name'"));
    }

    int32 RemovedCount = Blueprint->Animations.RemoveAll([&](UWidgetAnimation* Anim) {
        return Anim && Anim->GetName() == AnimationName;
    });

    if (RemovedCount > 0)
    {
        Blueprint->Modify();
        return FUmgMcpCommonUtils::CreateSuccessResponse();
    }
    return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Animation not found"));
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::SetPropertyKeys(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Called."));

    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) 
    {
        UE_LOG(LogUmgSequencer, Error, TEXT("SetPropertyKeys: Failed to get blueprint. %s"), *ErrorMessage);
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    // 1. Resolve Context
    FString AnimationName, WidgetName;
    if (GEditor)
    {
        if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
        {
            AnimationName = Sub->GetTargetAnimation();
            WidgetName = Sub->GetTargetWidget();
        }
    }

    UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Context - Animation: '%s', Widget: '%s'"), *AnimationName, *WidgetName);

    if (AnimationName.IsEmpty() || WidgetName.IsEmpty())
    {
        UE_LOG(LogUmgSequencer, Error, TEXT("SetPropertyKeys: Missing context."));
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing Animation or Widget context. Use set_animation_scope/set_widget_scope first."));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName) || PropertyName.IsEmpty())
    {
        UE_LOG(LogUmgSequencer, Error, TEXT("SetPropertyKeys: Missing 'property_name'."));
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name'"));
    }

    const TArray<TSharedPtr<FJsonValue>>* KeysPtr;
    if (!Params->TryGetArrayField(TEXT("keys"), KeysPtr))
    {
        UE_LOG(LogUmgSequencer, Error, TEXT("SetPropertyKeys: Missing 'keys' array."));
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'keys' array"));
    }

    // 2. Find Animation & MovieScene
    UWidgetAnimation* TargetAnimation = nullptr;
    for (UWidgetAnimation* Anim : Blueprint->Animations)
    {
        if (Anim && Anim->GetName() == AnimationName)
        {
            TargetAnimation = Anim;
            break;
        }
    }
    if (!TargetAnimation) 
    {
        UE_LOG(LogUmgSequencer, Error, TEXT("SetPropertyKeys: Animation '%s' not found in Blueprint."), *AnimationName);
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Animation not found"));
    }
    UMovieScene* MovieScene = TargetAnimation->GetMovieScene();
    
    // Mark for modification
    MovieScene->Modify();

    // 3. Find/Create Binding
    FGuid WidgetGuid;
    for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
    {
        if (MovieScene->GetPossessable(i).GetName() == WidgetName)
        {
            WidgetGuid = MovieScene->GetPossessable(i).GetGuid();
            break;
        }
    }

    if (!WidgetGuid.IsValid())
    {
        UWidget* Widget = Blueprint->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Widget not found in tree"));
        
        // ROBUSTNESS FIX: Ensure the widget is a variable and has a GUID in the Blueprint.
        // This handles cases where the widget was created manually or by older code without a GUID.
        if (!Widget->bIsVariable || !Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
        {
            UE_LOG(LogUmgSequencer, Warning, TEXT("SetPropertyKeys: Widget '%s' is missing GUID or not a variable. Auto-fixing..."), *WidgetName);
            
            if (!Widget->bIsVariable)
            {
                Widget->bIsVariable = true;
                UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Marked '%s' as variable."), *WidgetName);
            }
            
            if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
            {
                FGuid NewGuid = FGuid::NewGuid();
                Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), NewGuid);
                UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Assigned new GUID %s to '%s'."), *NewGuid.ToString(), *WidgetName);
            }
            Blueprint->Modify();
            
            // CRITICAL: Force compile to ensure the new variable property is generated in the class.
            // Without this, the animation binding may fail to resolve at runtime.
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
        }

        // 1. Add Possessable to MovieScene
        WidgetGuid = MovieScene->AddPossessable(WidgetName, Widget->GetClass());
        
        // 2. Add Binding to WidgetBlueprint (CRITICAL for UMG to see the track)
        FWidgetAnimationBinding NewBinding;
        NewBinding.WidgetName = FName(*WidgetName);
        NewBinding.AnimationGuid = WidgetGuid;
        NewBinding.bIsRootWidget = (Widget == Blueprint->WidgetTree->RootWidget);
        TargetAnimation->AnimationBindings.Add(NewBinding);
    }

    // 4. Find/Create Track
    UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid, FName(*PropertyName));
    if (!Track)
    {
        Track = MovieScene->AddTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid);
        if (auto* FloatTrack = Cast<UMovieSceneFloatTrack>(Track))
        {
            FloatTrack->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
            FloatTrack->SetDisplayName(FText::FromString(PropertyName));
        }
    }
    
    Track->Modify();

    auto* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
    if (!FloatTrack) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Track type mismatch (only Float supported currently)"));

    // 5. Apply Keys
    bool bSectionAdded = false;
    UMovieSceneSection* Section = FloatTrack->FindOrAddSection(0, bSectionAdded);
    auto* FloatSection = Cast<UMovieSceneFloatSection>(Section);

    if (FloatSection)
    {
        FloatSection->SetRange(TRange<FFrameNumber>::All());

        FFrameRate TickResolution = MovieScene->GetTickResolution();
        UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: TickResolution: %d / %d"), TickResolution.Numerator, TickResolution.Denominator);

        for (const auto& Val : *KeysPtr)
        {
            const TSharedPtr<FJsonObject>& KeyObj = Val->AsObject();
            if (!KeyObj.IsValid()) continue;

            double Time = KeyObj->GetNumberField(TEXT("time"));
            double Value = KeyObj->GetNumberField(TEXT("value"));
            
            FFrameNumber FrameNumber = (Time * TickResolution).RoundToFrame();
            UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Key - Time: %f, Value: %f, Frame: %d"), Time, Value, FrameNumber.Value);

            FloatSection->GetChannel().AddCubicKey(FrameNumber, Value);
            
            // Expand Range
            TRange<FFrameNumber> CurrentRange = MovieScene->GetPlaybackRange();
            FFrameNumber NewStart = CurrentRange.GetLowerBoundValue();
            FFrameNumber NewEnd = CurrentRange.GetUpperBoundValue();

            // If range is empty or default, initialize with first key
            if (CurrentRange.IsEmpty() || (NewStart == 0 && NewEnd == 0))
            {
                NewStart = FrameNumber;
                NewEnd = FrameNumber;
            }
            else
            {
                if (FrameNumber < NewStart) NewStart = FrameNumber;
                if (FrameNumber > NewEnd) NewEnd = FrameNumber;
            }
            
            UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Updating Range to [%d, %d]"), NewStart.Value, NewEnd.Value);
            MovieScene->SetPlaybackRange(TRange<FFrameNumber>(NewStart, NewEnd));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    
    TRange<FFrameNumber> FinalRange = MovieScene->GetPlaybackRange();
    UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Successfully applied %d keys to '%s.%s'. New Animation Range: [%d, %d]"), 
        KeysPtr->Num(), *WidgetName, *PropertyName, FinalRange.GetLowerBoundValue().Value, FinalRange.GetUpperBoundValue().Value);

    return FUmgMcpCommonUtils::CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::RemovePropertyTrack(const TSharedPtr<FJsonObject>& Params)
{
    // TODO: Implement
    return FUmgMcpCommonUtils::CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::RemoveKeys(const TSharedPtr<FJsonObject>& Params)
{
    // TODO: Implement
    return FUmgMcpCommonUtils::CreateSuccessResponse();
}
