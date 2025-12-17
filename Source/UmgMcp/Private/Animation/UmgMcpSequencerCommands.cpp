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
// Additional Sequencer Includes
#include "Tracks/MovieSceneColorTrack.h"
#include "Sections/MovieSceneColorSection.h"
#include "Tracks/MovieSceneVectorTrack.h" 
#include "Sections/MovieSceneVectorSection.h"

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
    if (Command == TEXT("set_animation_data")) return SetAnimationData(Params);

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

// Helper to determine value type from JSON Key object
enum class EKeyType { Float, Vector2D, Color, Unknown };

static EKeyType DetectKeyType(const TSharedPtr<FJsonObject>& KeyObj)
{
    if (KeyObj->HasField(TEXT("value")))
    {
        TSharedPtr<FJsonValue> Val = KeyObj->GetField<EJson::None>(TEXT("value"));
        if (Val->Type == EJson::Number) return EKeyType::Float;
        if (Val->Type == EJson::Object)
        {
            auto Obj = Val->AsObject();
            if (Obj->HasField(TEXT("r")) && Obj->HasField(TEXT("g"))) return EKeyType::Color;
            if (Obj->HasField(TEXT("x")) && Obj->HasField(TEXT("y"))) return EKeyType::Vector2D;
        }
    }
    return EKeyType::Unknown;
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::SetPropertyKeys(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("SetPropertyKeys: Called."));
    
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);

    // 1. Resolve Context
    FString AnimationName, WidgetName;
    if (Params->HasField(TEXT("animation_name"))) Params->TryGetStringField(TEXT("animation_name"), AnimationName);
    if (Params->HasField(TEXT("widget_name"))) Params->TryGetStringField(TEXT("widget_name"), WidgetName);

    if (GEditor && (AnimationName.IsEmpty() || WidgetName.IsEmpty()))
    {
        if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
        {
            if (AnimationName.IsEmpty()) AnimationName = Sub->GetTargetAnimation();
            if (WidgetName.IsEmpty()) WidgetName = Sub->GetTargetWidget();
        }
    }

    if (AnimationName.IsEmpty() || WidgetName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing Animation or Widget context."));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName) || PropertyName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name'"));
    }

    const TArray<TSharedPtr<FJsonValue>>* KeysPtr;
    if (!Params->TryGetArrayField(TEXT("keys"), KeysPtr))
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'keys' array"));
    }

    if (KeysPtr->Num() == 0) return FUmgMcpCommonUtils::CreateSuccessResponse();

    // 2. Detect Type from first key
    EKeyType KeyType = DetectKeyType((*KeysPtr)[0]->AsObject());
    if (KeyType == EKeyType::Unknown) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Could not detect key value type (Float, Vector2D, Color)"));

    // 3. Find Animation & MovieScene
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
    MovieScene->Modify();

    // 4. Find/Create Binding
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
        
        if (!Widget->bIsVariable || !Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
        {
            if (!Widget->bIsVariable) Widget->bIsVariable = true;
            if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
            {
                Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
            }
            Blueprint->Modify();
            FKismetEditorUtilities::CompileBlueprint(Blueprint); 
        }

        WidgetGuid = MovieScene->AddPossessable(WidgetName, Widget->GetClass());
        
        FWidgetAnimationBinding NewBinding;
        NewBinding.WidgetName = FName(*WidgetName);
        NewBinding.AnimationGuid = WidgetGuid;
        NewBinding.bIsRootWidget = (Widget == Blueprint->WidgetTree->RootWidget);
        TargetAnimation->AnimationBindings.Add(NewBinding);
    }

    // 5. Track Handling based on Type
    FFrameRate TickResolution = MovieScene->GetTickResolution();
    TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
    FFrameNumber RangeStart = PlaybackRange.GetLowerBoundValue();
    FFrameNumber RangeEnd = PlaybackRange.GetUpperBoundValue();
    bool bRangeUpdated = false;

    auto UpdateRange = [&](FFrameNumber Frame) {
        if (PlaybackRange.IsEmpty()) { RangeStart = Frame; RangeEnd = Frame; PlaybackRange = TRange<FFrameNumber>(RangeStart, RangeEnd); bRangeUpdated = true; return; }
        if (Frame < RangeStart) { RangeStart = Frame; bRangeUpdated = true; }
        if (Frame > RangeEnd) { RangeEnd = Frame; bRangeUpdated = true; }
    };

    if (KeyType == EKeyType::Float)
    {
        UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid, FName(*PropertyName));
        if (!Track)
        {
            Track = MovieScene->AddTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid);
            Cast<UMovieSceneFloatTrack>(Track)->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
        }
        Track->Modify();
        
        bool bSectionAdded = false;
        UMovieSceneSection* Section = Cast<UMovieSceneFloatTrack>(Track)->FindOrAddSection(0, bSectionAdded);
        Section->SetRange(TRange<FFrameNumber>::All());
        auto& Channel = Cast<UMovieSceneFloatSection>(Section)->GetChannel();

        for (const auto& Val : *KeysPtr)
        {
            auto KeyObj = Val->AsObject();
            double Time = KeyObj->GetNumberField(TEXT("time"));
            float Value = (float)KeyObj->GetNumberField(TEXT("value"));
            FFrameNumber Frame = (Time * TickResolution).RoundToFrame();
            Channel.AddCubicKey(Frame, Value);
            UpdateRange(Frame);
        }
    }
    else if (KeyType == EKeyType::Color)
    {
        UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneColorTrack::StaticClass(), WidgetGuid, FName(*PropertyName));
        if (!Track)
        {
            Track = MovieScene->AddTrack(UMovieSceneColorTrack::StaticClass(), WidgetGuid);
            Cast<UMovieSceneColorTrack>(Track)->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
        }
        Track->Modify();

        bool bSectionAdded = false;
        UMovieSceneSection* Section = Cast<UMovieSceneColorTrack>(Track)->FindOrAddSection(0, bSectionAdded);
        Section->SetRange(TRange<FFrameNumber>::All());
        auto* ColorSection = Cast<UMovieSceneColorSection>(Section);

        for (const auto& Val : *KeysPtr)
        {
            auto KeyObj = Val->AsObject();
            double Time = KeyObj->GetNumberField(TEXT("time"));
            auto ValueObj = KeyObj->GetObjectField(TEXT("value"));
            
            FLinearColor Color;
            Color.R = ValueObj->GetNumberField(TEXT("r"));
            Color.G = ValueObj->GetNumberField(TEXT("g"));
            Color.B = ValueObj->GetNumberField(TEXT("b"));
            Color.A = ValueObj->GetNumberField(TEXT("a"));

            FFrameNumber Frame = (Time * TickResolution).RoundToFrame();
            
            ColorSection->GetRedChannel().AddLinearKey(Frame, Color.R);
            ColorSection->GetGreenChannel().AddLinearKey(Frame, Color.G);
            ColorSection->GetBlueChannel().AddLinearKey(Frame, Color.B);
            ColorSection->GetAlphaChannel().AddLinearKey(Frame, Color.A);
            UpdateRange(Frame);
        }
    }
    else if (KeyType == EKeyType::Vector2D)
    {
        UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneDoubleVectorTrack::StaticClass(), WidgetGuid, FName(*PropertyName));
        if (!Track)
        {
            Track = MovieScene->AddTrack(UMovieSceneDoubleVectorTrack::StaticClass(), WidgetGuid);
            Cast<UMovieSceneDoubleVectorTrack>(Track)->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
            Cast<UMovieSceneDoubleVectorTrack>(Track)->SetNumChannelsUsed(2); 
        }
        Track->Modify();

        bool bSectionAdded = false;
        UMovieSceneSection* Section = Cast<UMovieSceneDoubleVectorTrack>(Track)->FindOrAddSection(0, bSectionAdded);
        Section->SetRange(TRange<FFrameNumber>::All());
        
        auto* VectorSection = Cast<UMovieSceneDoubleVectorSection>(Section);
        if (VectorSection)
        {
            VectorSection->SetChannelsUsed(2);

            for (const auto& Val : *KeysPtr)
            {
                auto KeyObj = Val->AsObject();
                double Time = KeyObj->GetNumberField(TEXT("time"));
                auto ValueObj = KeyObj->GetObjectField(TEXT("value"));
                
                double X = ValueObj->GetNumberField(TEXT("x"));
                double Y = ValueObj->GetNumberField(TEXT("y"));

                FFrameNumber Frame = (Time * TickResolution).RoundToFrame();
                
                // Use ChannelProxy to get mutable access to channels (Fix for C2662)
                FMovieSceneChannelProxy& Proxy = VectorSection->GetChannelProxy();
                TArrayView<FMovieSceneDoubleChannel*> Channels = Proxy.GetChannels<FMovieSceneDoubleChannel>();
                
                if (Channels.Num() >= 2)
                {
                    Channels[0]->AddLinearKey(Frame, X);
                    Channels[1]->AddLinearKey(Frame, Y);
                }
                UpdateRange(Frame);
            }
        }
    }

    if (bRangeUpdated)
    {
        MovieScene->SetPlaybackRange(TRange<FFrameNumber>(RangeStart, RangeEnd));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return FUmgMcpCommonUtils::CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::RemovePropertyTrack(const TSharedPtr<FJsonObject>& Params)
{
    // Simplified Implementation: Removes all tracks of a property for the scoped widget
    FString ErrorMessage;
    UWidgetBlueprint* Blueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!Blueprint) return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);

    // Resolve Context (Animation & Widget) ... (Similar to SetPropertyKeys)
     FString AnimationName, WidgetName;
    if (Params->HasField(TEXT("animation_name"))) Params->TryGetStringField(TEXT("animation_name"), AnimationName);
    if (Params->HasField(TEXT("widget_name"))) Params->TryGetStringField(TEXT("widget_name"), WidgetName);
    if (GEditor && (AnimationName.IsEmpty() || WidgetName.IsEmpty()))
    {
        if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
        {
            if (AnimationName.IsEmpty()) AnimationName = Sub->GetTargetAnimation();
            if (WidgetName.IsEmpty()) WidgetName = Sub->GetTargetWidget();
        }
    }
    if (AnimationName.IsEmpty() || WidgetName.IsEmpty()) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing context"));

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName)) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing property_name"));

    // Find Animation
    UWidgetAnimation* TargetAnimation = nullptr;
    for (UWidgetAnimation* Anim : Blueprint->Animations) { if (Anim && Anim->GetName() == AnimationName) { TargetAnimation = Anim; break; } }
    if (!TargetAnimation) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Animation not found"));

    UMovieScene* MovieScene = TargetAnimation->GetMovieScene();
    MovieScene->Modify();

    // Find Binding GUID
    FGuid WidgetGuid;
    for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
    {
        if (MovieScene->GetPossessable(i).GetName() == WidgetName)
        {
            WidgetGuid = MovieScene->GetPossessable(i).GetGuid();
            break;
        }
    }
    if (!WidgetGuid.IsValid()) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Widget binding not found in animation"));

    // Remove Track
    bool bFound = false;
    
    TArray<TSubclassOf<UMovieSceneTrack>> TrackTypes = { 
        UMovieSceneFloatTrack::StaticClass(), 
        UMovieSceneColorTrack::StaticClass(), 
        UMovieSceneDoubleVectorTrack::StaticClass() 
    };

    for (auto Class : TrackTypes)
    {
        UMovieSceneTrack* Track = MovieScene->FindTrack(Class, WidgetGuid, FName(*PropertyName));
        if (Track)
        {
            MovieScene->RemoveTrack(*Track);
            bFound = true;
        }
    }

    if (bFound)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        return FUmgMcpCommonUtils::CreateSuccessResponse();
    }
    
    return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Track not found"));
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::RemoveKeys(const TSharedPtr<FJsonObject>& Params)
{
    // For now, just alias to RemovePropertyTrack as granular key removal is complex
    // User can just overwrite keys by setting them again
    return RemovePropertyTrack(Params);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::SetAnimationData(const TSharedPtr<FJsonObject>& Params)
{
    // Level 2 API: Batch process
    FString WidgetName, AnimationName;
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName)) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing widget_name"));
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName)) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing animation_name"));

    const TArray<TSharedPtr<FJsonValue>>* TracksPtr;
    if (!Params->TryGetArrayField(TEXT("tracks"), TracksPtr)) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing tracks array"));

    for (const auto& TrackVal : *TracksPtr)
    {
        TSharedPtr<FJsonObject> TrackObj = TrackVal->AsObject();
        if (!TrackObj.IsValid()) continue;

        // Construct params for SetPropertyKeys
        TSharedPtr<FJsonObject> SubParams = MakeShared<FJsonObject>();
        SubParams->SetStringField(TEXT("widget_name"), WidgetName);
        SubParams->SetStringField(TEXT("animation_name"), AnimationName);
        SubParams->SetStringField(TEXT("property_name"), TrackObj->GetStringField(TEXT("property"))); 
        SubParams->SetArrayField(TEXT("keys"), TrackObj->GetArrayField(TEXT("keys")));

        if (Params->HasField(TEXT("asset_path"))) SubParams->SetStringField(TEXT("asset_path"), Params->GetStringField(TEXT("asset_path")));

        SetPropertyKeys(SubParams);
    }

    return FUmgMcpCommonUtils::CreateSuccessResponse();
}
