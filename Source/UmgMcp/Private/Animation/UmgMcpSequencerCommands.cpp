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

// ... (Keep existing Attention section)
// ... (Keep existing Read section: GetAllAnimations, GetAnimationKeyframes, GetAnimatedWidgets, GetAnimationFullData, GetWidgetAnimationData)
// ... (Keep existing Write section: CreateAnimation, DeleteAnimation)

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
    if (!Blueprint) 
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    // 1. Resolve Context
    FString AnimationName, WidgetName;
    if (Params->HasField(TEXT("animation_name"))) Params->TryGetStringField(TEXT("animation_name"), AnimationName);
    if (Params->HasField(TEXT("widget_name"))) Params->TryGetStringField(TEXT("widget_name"), WidgetName);

    // Fallback to Attention Subsystem if missing
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
        
        // Ensure variable + GUID
        if (!Widget->bIsVariable || !Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
        {
            if (!Widget->bIsVariable) Widget->bIsVariable = true;
            if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
            {
                Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
            }
            Blueprint->Modify();
            FKismetEditorUtilities::CompileBlueprint(Blueprint); // Recompile to generate property
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
        // Use VectorTrack (User confirmed header availability)
        UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneVectorTrack::StaticClass(), WidgetGuid, FName(*PropertyName));
        if (!Track)
        {
            Track = MovieScene->AddTrack(UMovieSceneVectorTrack::StaticClass(), WidgetGuid);
            Cast<UMovieSceneVectorTrack>(Track)->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
            // Default to 2 channels
            Cast<UMovieSceneVectorTrack>(Track)->SetNumChannelsUsed(2); 
        }
        Track->Modify();

        bool bSectionAdded = false;
        UMovieSceneSection* Section = Cast<UMovieSceneVectorTrack>(Track)->FindOrAddSection(0, bSectionAdded);
        Section->SetRange(TRange<FFrameNumber>::All());
        auto* VectorSection = Cast<UMovieSceneVectorSection>(Section);
        // Ensure channels 0 and 1 are active
        VectorSection->SetChannelsUsed(2);

        for (const auto& Val : *KeysPtr)
        {
            auto KeyObj = Val->AsObject();
            double Time = KeyObj->GetNumberField(TEXT("time"));
            auto ValueObj = KeyObj->GetObjectField(TEXT("value"));
            
            double X = ValueObj->GetNumberField(TEXT("x"));
            double Y = ValueObj->GetNumberField(TEXT("y"));

            FFrameNumber Frame = (Time * TickResolution).RoundToFrame();
            
            // Channel 0 = X, Channel 1 = Y
            // Note: VectorTrack usually takes floats or doubles depending on version. 
            // AddLinearKey overloads should handle it.
            VectorSection->GetChannel(0).AddLinearKey(Frame, X);
            VectorSection->GetChannel(1).AddLinearKey(Frame, Y);
            UpdateRange(Frame);
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
    
    // We need to search all tracks bound to this GUID that match the property name
    // Since we don't know the exact class (Float/Vector/Color), we check PropertyName
    // Note: MovieScene->FindTrack requires a class. We can iterate bindings.
    
    // Hacky: Try finding specific types. Ideally iterating all tracks would be better but UE API is strict.
    // Let's check Float, Vector, Color
    TArray<TSubclassOf<UMovieSceneTrack>> TrackTypes = { 
        UMovieSceneFloatTrack::StaticClass(), 
        UMovieSceneColorTrack::StaticClass(), 
        UMovieSceneVectorTrack::StaticClass() 
    };

    for (auto Class : TrackTypes)
    {
        UMovieSceneTrack* Track = MovieScene->FindTrack(Class, WidgetGuid, FName(*PropertyName));
        if (Track)
        {
            MovieScene->RemoveTrack(Track);
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
    /*
    {
        "widget_name": "MyButton",
        "animation_name": "Anim1",
        "tracks": [
            { "property": "RenderOpacity", "keys": [...] },
            { "property": "RenderTransform.Translation", "keys": [...] }
        ]
    }
    */
    
    // We delegate to SetPropertyKeys for each track
    // We construct a fake Params object for each call
    
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
        SubParams->SetStringField(TEXT("property_name"), TrackObj->GetStringField(TEXT("property"))); // "property" in input -> "property_name" in SetPropertyKeys
        SubParams->SetArrayField(TEXT("keys"), TrackObj->GetArrayField(TEXT("keys")));

        // Optional: asset_path if needed
        if (Params->HasField(TEXT("asset_path"))) SubParams->SetStringField(TEXT("asset_path"), Params->GetStringField(TEXT("asset_path")));

        SetPropertyKeys(SubParams);
    }

    return FUmgMcpCommonUtils::CreateSuccessResponse();
}
