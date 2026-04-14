// Copyright (c) 2025-2026 Winyunq. All rights reserved.
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
#include "Subsystems/AssetEditorSubsystem.h"

// Sequencer Includes
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
// Additional Sequencer Includes
#include "Tracks/MovieSceneColorTrack.h"
#include "Sections/MovieSceneColorSection.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Algo/Sort.h"

// Define a log category for Sequencer commands
DEFINE_LOG_CATEGORY_STATIC(LogUmgSequencer, Log, All);

FUmgMcpSequencerCommands::FUmgMcpSequencerCommands()
{
}

FUmgMcpSequencerCommands::~FUmgMcpSequencerCommands()
{
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::ResolveAnimationContext(
    const TSharedPtr<FJsonObject>& Params,
    UWidgetBlueprint*& OutBlueprint,
    UWidgetAnimation*& OutAnimation,
    FString& OutError) const
{
    FString ErrorMessage;
    OutBlueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
    if (!OutBlueprint)
    {
        OutError = ErrorMessage;
        return FUmgMcpCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AnimationName;
    if (!Params->TryGetStringField(TEXT("animation_name"), AnimationName) || AnimationName.IsEmpty())
    {
        if (GEditor)
        {
            if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            {
                AnimationName = Sub->GetTargetAnimation();
            }
        }
    }

    if (AnimationName.IsEmpty())
    {
        OutError = TEXT("Missing 'animation_name'");
        return FUmgMcpCommonUtils::CreateErrorResponse(OutError);
    }

    OutAnimation = nullptr;
    for (UWidgetAnimation* Anim : OutBlueprint->Animations)
    {
        if (Anim && Anim->GetName() == AnimationName)
        {
            OutAnimation = Anim;
            break;
        }
    }

    if (!OutAnimation)
    {
        OutError = TEXT("Animation not found");
        return FUmgMcpCommonUtils::CreateErrorResponse(OutError);
    }

    OutError.Reset();
    return nullptr;
}

bool FUmgMcpSequencerCommands::ResolveWidgetName(const TSharedPtr<FJsonObject>& Params, FString& OutWidgetName) const
{
    if (Params->TryGetStringField(TEXT("widget_name"), OutWidgetName) && !OutWidgetName.IsEmpty())
    {
        return true;
    }

    if (GEditor)
    {
        if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
        {
            OutWidgetName = Sub->GetTargetWidget();
        }
    }
    return !OutWidgetName.IsEmpty();
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
    // Attention
    if (Command == TEXT("set_animation_scope") || Command == TEXT("animation_target")) return SetAnimationTarget(Params);
    if (Command == TEXT("set_widget_scope") || Command == TEXT("widget_target")) return SetWidgetTarget(Params);

    // Read
    if (Command == TEXT("get_all_animations")) return GetAllAnimations(Params);
    if (Command == TEXT("get_animation_keyframes")) return GetAnimationKeyframes(Params);
    if (Command == TEXT("get_animated_widgets")) return GetAnimatedWidgets(Params);
    if (Command == TEXT("get_animation_full_data")) return GetAnimationFullData(Params);
    if (Command == TEXT("get_widget_animation_data")) return GetWidgetAnimationData(Params);
    if (Command == TEXT("animation_widget_properties")) return GetWidgetPropertyTimeline(Params);
    if (Command == TEXT("animation_time_properties")) return GetTimeSliceProperties(Params);
    if (Command == TEXT("animation_overview")) return GetAnimationOverview(Params);

    // Write
    if (Command == TEXT("create_animation")) return CreateAnimation(Params);
    if (Command == TEXT("delete_animation")) return DeleteAnimation(Params);
    if (Command == TEXT("set_property_keys")) return SetPropertyKeys(Params);
    if (Command == TEXT("remove_property_track")) return RemovePropertyTrack(Params);
    if (Command == TEXT("remove_keys")) return RemoveKeys(Params);
    if (Command == TEXT("set_animation_data")) return SetAnimationData(Params);
    if (Command == TEXT("animation_append_widget_tracks")) return AppendWidgetTracks(Params);
    if (Command == TEXT("animation_append_time_slice")) return AppendTimeSlice(Params);
    if (Command == TEXT("animation_delete_widget_keys")) return DeleteWidgetKeys(Params);

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

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::SetAnimationTarget(const TSharedPtr<FJsonObject>& Params)
{
    return SetAnimationScope(Params);
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
            TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
            Data->SetStringField(TEXT("widget_name"), WidgetName);
            return FUmgMcpCommonUtils::CreateSuccessResponse(Data);
        }
    }

    return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Failed to access UmgAttentionSubsystem"));
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::SetWidgetTarget(const TSharedPtr<FJsonObject>& Params)
{
    return SetWidgetScope(Params);
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
    // Reuse the richer widget timeline helper to provide focused data.
    return GetWidgetPropertyTimeline(Params);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetWidgetPropertyTimeline(const TSharedPtr<FJsonObject>& Params)
{
    UWidgetBlueprint* Blueprint = nullptr;
    UWidgetAnimation* Animation = nullptr;
    FString Error;
    if (TSharedPtr<FJsonObject> Err = ResolveAnimationContext(Params, Blueprint, Animation, Error)) return Err;

    FString WidgetName;
    const bool bHasWidgetFilter = ResolveWidgetName(Params, WidgetName);
    FString PropertyFilter;
    Params->TryGetStringField(TEXT("property_name"), PropertyFilter);

    UMovieScene* MovieScene = Animation->GetMovieScene();
    if (!MovieScene) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("MovieScene is null"));

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    TArray<TSharedPtr<FJsonValue>> Changes;

    auto AddChange = [&](const FString& InWidget, const FString& Property, const FString& ValueType, double TimeSeconds, const TSharedPtr<FJsonValue>& ValuePayload)
    {
        TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
        Change->SetStringField(TEXT("widget"), InWidget);
        Change->SetStringField(TEXT("property"), Property);
        Change->SetStringField(TEXT("value_type"), ValueType);
        Change->SetNumberField(TEXT("time"), TimeSeconds);
        Change->SetField(TEXT("value"), ValuePayload);
        Changes.Add(MakeShared<FJsonValueObject>(Change));
    };

    for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
    {
        const FString BindingWidget = Binding.WidgetName.ToString();
        if (bHasWidgetFilter && BindingWidget != WidgetName) continue;

        const FGuid ObjectGuid = Binding.AnimationGuid;

        // Float tracks
        for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneFloatTrack::StaticClass(), ObjectGuid))
        {
            const UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
            if (!FloatTrack) continue;
            const FString PropertyName = FloatTrack->GetPropertyName().ToString();
            if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

            for (const UMovieSceneSection* Section : FloatTrack->GetAllSections())
            {
                const UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section);
                if (!FloatSection) continue;

                const auto& Channel = FloatSection->GetChannel();
                auto Times = Channel.GetData().GetTimes();
                auto Values = Channel.GetData().GetValues();

                for (int32 i = 0; i < Times.Num(); ++i)
                {
                    AddChange(BindingWidget, PropertyName, TEXT("float"), TickResolution.AsSeconds(Times[i]), MakeShared<FJsonValueNumber>(Values[i].Value));
                }
            }
        }

        // Color tracks
        for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneColorTrack::StaticClass(), ObjectGuid))
        {
            const UMovieSceneColorTrack* ColorTrack = Cast<UMovieSceneColorTrack>(Track);
            if (!ColorTrack) continue;
            const FString PropertyName = ColorTrack->GetPropertyName().ToString();
            if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

            for (const UMovieSceneSection* Section : ColorTrack->GetAllSections())
            {
                const UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>(Section);
                if (!ColorSection) continue;

                const auto Times = ColorSection->GetRedChannel().GetData().GetTimes();
                const auto Reds = ColorSection->GetRedChannel().GetData().GetValues();
                const auto Greens = ColorSection->GetGreenChannel().GetData().GetValues();
                const auto Blues = ColorSection->GetBlueChannel().GetData().GetValues();
                const auto Alphas = ColorSection->GetAlphaChannel().GetData().GetValues();

                const int32 KeyCount = Times.Num();
                for (int32 i = 0; i < KeyCount; ++i)
                {
                    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
                    ColorObj->SetNumberField(TEXT("r"), Reds.IsValidIndex(i) ? Reds[i].Value : 0.f);
                    ColorObj->SetNumberField(TEXT("g"), Greens.IsValidIndex(i) ? Greens[i].Value : 0.f);
                    ColorObj->SetNumberField(TEXT("b"), Blues.IsValidIndex(i) ? Blues[i].Value : 0.f);
                    ColorObj->SetNumberField(TEXT("a"), Alphas.IsValidIndex(i) ? Alphas[i].Value : 1.f);

                    AddChange(BindingWidget, PropertyName, TEXT("color"), TickResolution.AsSeconds(Times[i]), MakeShared<FJsonValueObject>(ColorObj));
                }
            }
        }

        // Vector2D tracks
        for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneDoubleVectorTrack::StaticClass(), ObjectGuid))
        {
            const UMovieSceneDoubleVectorTrack* VectorTrack = Cast<UMovieSceneDoubleVectorTrack>(Track);
            if (!VectorTrack || VectorTrack->GetNumChannelsUsed() < 2) continue;

            const FString PropertyName = VectorTrack->GetPropertyName().ToString();
            if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

            for (const UMovieSceneSection* Section : VectorTrack->GetAllSections())
            {
                const UMovieSceneDoubleVectorSection* VectorSection = Cast<UMovieSceneDoubleVectorSection>(Section);
                if (!VectorSection) continue;

                FMovieSceneChannelProxy& Proxy = VectorSection->GetChannelProxy();
                auto Channels = Proxy.GetChannels<FMovieSceneDoubleChannel>();
                if (Channels.Num() < 2) continue;

                const auto Times = Channels[0]->GetData().GetTimes();
                const auto XValues = Channels[0]->GetData().GetValues();
                const auto YValues = Channels[1]->GetData().GetValues();

                const int32 KeyCount = Times.Num();
                for (int32 i = 0; i < KeyCount; ++i)
                {
                    TSharedPtr<FJsonObject> VecObj = MakeShared<FJsonObject>();
                    VecObj->SetNumberField(TEXT("x"), XValues.IsValidIndex(i) ? XValues[i].Value : 0.0);
                    VecObj->SetNumberField(TEXT("y"), YValues.IsValidIndex(i) ? YValues[i].Value : 0.0);
                    AddChange(BindingWidget, PropertyName, TEXT("vector2d"), TickResolution.AsSeconds(Times[i]), MakeShared<FJsonValueObject>(VecObj));
                }
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("animation"), Animation->GetName());
    if (bHasWidgetFilter) Result->SetStringField(TEXT("widget"), WidgetName);
    if (!PropertyFilter.IsEmpty()) Result->SetStringField(TEXT("property_filter"), PropertyFilter);
    Result->SetNumberField(TEXT("change_count"), Changes.Num());
    Result->SetArrayField(TEXT("changes"), Changes);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetTimeSliceProperties(const TSharedPtr<FJsonObject>& Params)
{
    UWidgetBlueprint* Blueprint = nullptr;
    UWidgetAnimation* Animation = nullptr;
    FString Error;
    if (TSharedPtr<FJsonObject> Err = ResolveAnimationContext(Params, Blueprint, Animation, Error)) return Err;

    UMovieScene* MovieScene = Animation->GetMovieScene();
    if (!MovieScene) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("MovieScene is null"));

    TArray<double> TimesSeconds;
    const TArray<TSharedPtr<FJsonValue>>* TimesArray = nullptr;
    if (Params->TryGetArrayField(TEXT("times"), TimesArray))
    {
        for (const auto& Val : *TimesArray)
        {
            if (Val->Type == EJson::Number) TimesSeconds.Add(Val->AsNumber());
        }
    }
    else
    {
        double SingleTime = 0.0;
        if (Params->TryGetNumberField(TEXT("time"), SingleTime))
        {
            TimesSeconds.Add(SingleTime);
        }
    }

    if (TimesSeconds.Num() == 0)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Provide 'time' or 'times' (seconds) to query key values."));
    }

    FString WidgetName;
    const bool bHasWidgetFilter = ResolveWidgetName(Params, WidgetName);
    FString PropertyFilter;
    Params->TryGetStringField(TEXT("property_name"), PropertyFilter);

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    TArray<TSharedPtr<FJsonValue>> Slices;

    for (double QueryTime : TimesSeconds)
    {
        FFrameNumber QueryFrame = (QueryTime * TickResolution).RoundToFrame();
        TArray<TSharedPtr<FJsonValue>> ValuesAtTime;

        for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
        {
            const FString BindingWidget = Binding.WidgetName.ToString();
            if (bHasWidgetFilter && BindingWidget != WidgetName) continue;
            const FGuid ObjectGuid = Binding.AnimationGuid;

            // Float
            for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneFloatTrack::StaticClass(), ObjectGuid))
            {
                const UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
                if (!FloatTrack) continue;
                const FString PropertyName = FloatTrack->GetPropertyName().ToString();
                if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

                for (const UMovieSceneSection* Section : FloatTrack->GetAllSections())
                {
                    const UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section);
                    if (!FloatSection) continue;

                    float EvalValue = 0.f;
                    if (FloatSection->GetChannel().Evaluate(QueryFrame, EvalValue))
                    {
                        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
                        Obj->SetStringField(TEXT("widget"), BindingWidget);
                        Obj->SetStringField(TEXT("property"), PropertyName);
                        Obj->SetStringField(TEXT("value_type"), TEXT("float"));
                        Obj->SetNumberField(TEXT("value"), EvalValue);
                        ValuesAtTime.Add(MakeShared<FJsonValueObject>(Obj));
                        break;
                    }
                }
            }

            // Color
            for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneColorTrack::StaticClass(), ObjectGuid))
            {
                const UMovieSceneColorTrack* ColorTrack = Cast<UMovieSceneColorTrack>(Track);
                if (!ColorTrack) continue;
                const FString PropertyName = ColorTrack->GetPropertyName().ToString();
                if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

                for (const UMovieSceneSection* Section : ColorTrack->GetAllSections())
                {
                    const UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>(Section);
                    if (!ColorSection) continue;

                    float R = 0.f, G = 0.f, B = 0.f, A = 1.f;
                    bool bHasValue = ColorSection->GetRedChannel().Evaluate(QueryFrame, R);
                    bHasValue |= ColorSection->GetGreenChannel().Evaluate(QueryFrame, G);
                    bHasValue |= ColorSection->GetBlueChannel().Evaluate(QueryFrame, B);
                    bHasValue |= ColorSection->GetAlphaChannel().Evaluate(QueryFrame, A);

                    if (bHasValue)
                    {
                        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
                        Obj->SetStringField(TEXT("widget"), BindingWidget);
                        Obj->SetStringField(TEXT("property"), PropertyName);
                        Obj->SetStringField(TEXT("value_type"), TEXT("color"));

                        TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
                        ColorObj->SetNumberField(TEXT("r"), R);
                        ColorObj->SetNumberField(TEXT("g"), G);
                        ColorObj->SetNumberField(TEXT("b"), B);
                        ColorObj->SetNumberField(TEXT("a"), A);
                        Obj->SetObjectField(TEXT("value"), ColorObj);

                        ValuesAtTime.Add(MakeShared<FJsonValueObject>(Obj));
                        break;
                    }
                }
            }

            // Vector2D
            for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneDoubleVectorTrack::StaticClass(), ObjectGuid))
            {
                const UMovieSceneDoubleVectorTrack* VectorTrack = Cast<UMovieSceneDoubleVectorTrack>(Track);
                if (!VectorTrack || VectorTrack->GetNumChannelsUsed() < 2) continue;

                const FString PropertyName = VectorTrack->GetPropertyName().ToString();
                if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

                for (const UMovieSceneSection* Section : VectorTrack->GetAllSections())
                {
                    const UMovieSceneDoubleVectorSection* VectorSection = Cast<UMovieSceneDoubleVectorSection>(Section);
                    if (!VectorSection) continue;

                FMovieSceneChannelProxy& Proxy = VectorSection->GetChannelProxy();
                auto Channels = Proxy.GetChannels<FMovieSceneDoubleChannel>();
                    if (Channels.Num() < 2) continue;

                    double X = 0.0, Y = 0.0;
                    bool bHasValue = Channels[0]->Evaluate(QueryFrame, X) | Channels[1]->Evaluate(QueryFrame, Y);
                    if (bHasValue)
                    {
                        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
                        Obj->SetStringField(TEXT("widget"), BindingWidget);
                        Obj->SetStringField(TEXT("property"), PropertyName);
                        Obj->SetStringField(TEXT("value_type"), TEXT("vector2d"));

                        TSharedPtr<FJsonObject> VecObj = MakeShared<FJsonObject>();
                        VecObj->SetNumberField(TEXT("x"), X);
                        VecObj->SetNumberField(TEXT("y"), Y);
                        Obj->SetObjectField(TEXT("value"), VecObj);

                        ValuesAtTime.Add(MakeShared<FJsonValueObject>(Obj));
                        break;
                    }
                }
            }
        }

        TSharedPtr<FJsonObject> Slice = MakeShared<FJsonObject>();
        Slice->SetNumberField(TEXT("time"), QueryTime);
        Slice->SetNumberField(TEXT("values_count"), ValuesAtTime.Num());
        Slice->SetArrayField(TEXT("values"), ValuesAtTime);
        Slices.Add(MakeShared<FJsonValueObject>(Slice));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("animation"), Animation->GetName());
    if (bHasWidgetFilter) Result->SetStringField(TEXT("widget"), WidgetName);
    if (!PropertyFilter.IsEmpty()) Result->SetStringField(TEXT("property_filter"), PropertyFilter);
    Result->SetNumberField(TEXT("slice_count"), Slices.Num());
    Result->SetArrayField(TEXT("slices"), Slices);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::GetAnimationOverview(const TSharedPtr<FJsonObject>& Params)
{
    UWidgetBlueprint* Blueprint = nullptr;
    UWidgetAnimation* Animation = nullptr;
    FString Error;
    if (TSharedPtr<FJsonObject> Err = ResolveAnimationContext(Params, Blueprint, Animation, Error)) return Err;

    UMovieScene* MovieScene = Animation->GetMovieScene();
    if (!MovieScene) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("MovieScene is null"));

    FString WidgetName;
    const bool bHasWidgetFilter = ResolveWidgetName(Params, WidgetName);
    FString PropertyFilter;
    Params->TryGetStringField(TEXT("property_name"), PropertyFilter);

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    TSet<FFrameNumber> UniqueFrames;
    TArray<TSharedPtr<FJsonValue>> ChangedProperties;

    auto AccumulateTimes = [&](const FString& InWidget, const FString& PropertyName, const FString& TrackType, const TArrayView<const FFrameNumber>& Times)
    {
        for (const FFrameNumber& Frame : Times)
        {
            UniqueFrames.Add(Frame);
        }

        TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
        PropObj->SetStringField(TEXT("widget"), InWidget);
        PropObj->SetStringField(TEXT("property"), PropertyName);
        PropObj->SetStringField(TEXT("track_type"), TrackType);
        PropObj->SetNumberField(TEXT("keys_count"), Times.Num());
        ChangedProperties.Add(MakeShared<FJsonValueObject>(PropObj));
    };

    for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
    {
        const FString BindingWidget = Binding.WidgetName.ToString();
        if (bHasWidgetFilter && BindingWidget != WidgetName) continue;
        const FGuid ObjectGuid = Binding.AnimationGuid;

        for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneFloatTrack::StaticClass(), ObjectGuid))
        {
            const UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
            if (!FloatTrack) continue;
            const FString PropertyName = FloatTrack->GetPropertyName().ToString();
            if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

            for (const UMovieSceneSection* Section : FloatTrack->GetAllSections())
            {
                const UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section);
                if (!FloatSection) continue;
                AccumulateTimes(BindingWidget, PropertyName, TEXT("float"), FloatSection->GetChannel().GetData().GetTimes());
            }
        }

        for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneColorTrack::StaticClass(), ObjectGuid))
        {
            const UMovieSceneColorTrack* ColorTrack = Cast<UMovieSceneColorTrack>(Track);
            if (!ColorTrack) continue;
            const FString PropertyName = ColorTrack->GetPropertyName().ToString();
            if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

            for (const UMovieSceneSection* Section : ColorTrack->GetAllSections())
            {
                const UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>(Section);
                if (!ColorSection) continue;
                AccumulateTimes(BindingWidget, PropertyName, TEXT("color"), ColorSection->GetRedChannel().GetData().GetTimes());
            }
        }

        for (const UMovieSceneTrack* Track : MovieScene->FindTracks(UMovieSceneDoubleVectorTrack::StaticClass(), ObjectGuid))
        {
            const UMovieSceneDoubleVectorTrack* VectorTrack = Cast<UMovieSceneDoubleVectorTrack>(Track);
            if (!VectorTrack || VectorTrack->GetNumChannelsUsed() < 2) continue;
            const FString PropertyName = VectorTrack->GetPropertyName().ToString();
            if (!PropertyFilter.IsEmpty() && PropertyName != PropertyFilter) continue;

            for (const UMovieSceneSection* Section : VectorTrack->GetAllSections())
            {
                const UMovieSceneDoubleVectorSection* VectorSection = Cast<UMovieSceneDoubleVectorSection>(Section);
                if (!VectorSection) continue;
                FMovieSceneChannelProxy& Proxy = VectorSection->GetChannelProxy();
                TArrayView<FMovieSceneDoubleChannel*> Channels = Proxy.GetChannels<FMovieSceneDoubleChannel>();
                if (Channels.Num() < 2) continue;
                AccumulateTimes(BindingWidget, PropertyName, TEXT("vector2d"), Channels[0]->GetData().GetTimes());
            }
        }
    }

    TArray<double> KeyTimesSeconds;
    KeyTimesSeconds.Reserve(UniqueFrames.Num());
    for (const FFrameNumber& Frame : UniqueFrames)
    {
        KeyTimesSeconds.Add(TickResolution.AsSeconds(Frame));
    }
    Algo::Sort(KeyTimesSeconds);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("animation"), Animation->GetName());
    if (bHasWidgetFilter) Result->SetStringField(TEXT("widget"), WidgetName);
    if (!PropertyFilter.IsEmpty()) Result->SetStringField(TEXT("property_filter"), PropertyFilter);
    Result->SetNumberField(TEXT("track_count"), ChangedProperties.Num());
    Result->SetNumberField(TEXT("keyframe_count"), KeyTimesSeconds.Num());
    TArray<TSharedPtr<FJsonValue>> KeyTimesJson;
    for (double TimeSeconds : KeyTimesSeconds)
    {
        KeyTimesJson.Add(MakeShared<FJsonValueNumber>(TimeSeconds));
    }
    Result->SetArrayField(TEXT("key_times"), KeyTimesJson);
    Result->SetArrayField(TEXT("changed_properties"), ChangedProperties);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
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
            TSharedPtr<FJsonObject> ExistResult = MakeShared<FJsonObject>();
            ExistResult->SetStringField(TEXT("name"), AnimationName);
            ExistResult->SetStringField(TEXT("action"), TEXT("found"));
            return FUmgMcpCommonUtils::CreateSuccessResponse(ExistResult);
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

    // Force open/refresh the asset editor
    if (GEditor)
    {
        if (auto* AssetSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetSub->OpenEditorForAsset(Blueprint);
        }
    }

    UE_LOG(LogUmgSequencer, Log, TEXT("CreateAnimation: Successfully created animation '%s'."), *NewAnimation->GetName());
    
    // Attempt to set focus (this sets internal state for AI context)
    if (GEditor)
    {
        if (auto* Sub = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            Sub->SetTargetAnimation(AnimationName);
    }
    
    // ... Result construction ...
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewAnimation->GetName());
    Result->SetStringField(TEXT("action"), TEXT("created"));
    Result->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
    
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

    bool bConfirmed = false;
    Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmed);
    if (!bConfirmed)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Deletion hardened (Issue 15): set 'confirm_delete': true to delete an animation explicitly."));
    }

    int32 RemovedCount = Blueprint->Animations.RemoveAll([&](UWidgetAnimation* Anim) {
        return Anim && Anim->GetName() == AnimationName;
    });

    if (RemovedCount > 0)
    {
        Blueprint->Modify();
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("deleted_animation"), AnimationName);
        return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
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
    
    // Refresh Editor
    if (GEditor)
    {
        if (auto* AssetSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetSub->OpenEditorForAsset(Blueprint);
        }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("animation"), AnimationName);
    Result->SetStringField(TEXT("widget"), WidgetName);
    Result->SetStringField(TEXT("property"), PropertyName);
    Result->SetNumberField(TEXT("keys_count"), KeysPtr->Num());
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::AppendWidgetTracks(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("AppendWidgetTracks: Called."));

    UWidgetBlueprint* Blueprint = nullptr;
    UWidgetAnimation* Animation = nullptr;
    FString Error;
    if (TSharedPtr<FJsonObject> Err = ResolveAnimationContext(Params, Blueprint, Animation, Error)) return Err;

    FString WidgetName;
    if (!ResolveWidgetName(Params, WidgetName))
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' for append."));
    }

    const TArray<TSharedPtr<FJsonValue>>* TracksPtr = nullptr;
    if (!Params->TryGetArrayField(TEXT("tracks"), TracksPtr) || !TracksPtr)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'tracks' array."));
    }

    int32 KeysTotal = 0;
    TArray<TSharedPtr<FJsonValue>> TrackSummaries;

    for (const TSharedPtr<FJsonValue>& TrackVal : *TracksPtr)
    {
        TSharedPtr<FJsonObject> TrackObj = TrackVal->AsObject();
        if (!TrackObj.IsValid()) continue;

        FString PropertyName;
        if (!TrackObj->TryGetStringField(TEXT("property"), PropertyName) || PropertyName.IsEmpty())
        {
            return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Each track needs a 'property' name."));
        }

        const TArray<TSharedPtr<FJsonValue>>* KeysPtr = nullptr;
        if (!TrackObj->TryGetArrayField(TEXT("keys"), KeysPtr))
        {
            return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Track is missing 'keys' array."));
        }

        TSharedPtr<FJsonObject> SubParams = MakeShared<FJsonObject>();
        SubParams->SetStringField(TEXT("animation_name"), Animation->GetName());
        SubParams->SetStringField(TEXT("widget_name"), WidgetName);
        SubParams->SetStringField(TEXT("property_name"), PropertyName);
        SubParams->SetArrayField(TEXT("keys"), *KeysPtr);

        if (Params->HasField(TEXT("asset_path")))
        {
            FString AssetPath;
            if (Params->TryGetStringField(TEXT("asset_path"), AssetPath))
            {
                SubParams->SetStringField(TEXT("asset_path"), AssetPath);
            }
        }

        TSharedPtr<FJsonObject> SubResult = SetPropertyKeys(SubParams);
        if (!SubResult.IsValid() || !SubResult->GetBoolField(TEXT("success")))
        {
            return SubResult.IsValid() ? SubResult : FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Failed to apply track keys."));
        }

        KeysTotal += KeysPtr->Num();

        TSharedPtr<FJsonObject> TrackSummary = MakeShared<FJsonObject>();
        TrackSummary->SetStringField(TEXT("property"), PropertyName);
        TrackSummary->SetNumberField(TEXT("keys_applied"), KeysPtr->Num());
        TrackSummaries.Add(MakeShared<FJsonValueObject>(TrackSummary));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("animation"), Animation->GetName());
    Result->SetStringField(TEXT("widget"), WidgetName);
    Result->SetNumberField(TEXT("track_count"), TrackSummaries.Num());
    Result->SetNumberField(TEXT("keys_total"), KeysTotal);
    Result->SetArrayField(TEXT("tracks"), TrackSummaries);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::AppendTimeSlice(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgSequencer, Log, TEXT("AppendTimeSlice: Called."));

    UWidgetBlueprint* Blueprint = nullptr;
    UWidgetAnimation* Animation = nullptr;
    FString Error;
    if (TSharedPtr<FJsonObject> Err = ResolveAnimationContext(Params, Blueprint, Animation, Error)) return Err;

    double TimeSeconds = 0.0;
    if (!Params->TryGetNumberField(TEXT("time"), TimeSeconds))
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'time' (seconds) for append_time_slice."));
    }

    const TArray<TSharedPtr<FJsonValue>>* WidgetsPtr = nullptr;
    if (!Params->TryGetArrayField(TEXT("widgets"), WidgetsPtr) || !WidgetsPtr)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'widgets' array with properties to write."));
    }

    int32 KeysTotal = 0;
    TArray<TSharedPtr<FJsonValue>> SliceSummaries;

    for (const TSharedPtr<FJsonValue>& WidgetVal : *WidgetsPtr)
    {
        TSharedPtr<FJsonObject> WidgetObj = WidgetVal->AsObject();
        if (!WidgetObj.IsValid()) continue;

        FString WidgetName;
        if (!WidgetObj->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
        {
            return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Each entry needs a 'widget_name'."));
        }

        TSharedPtr<FJsonObject> PropertiesObj;
        const TSharedPtr<FJsonObject>* PropertiesObjectPtr = nullptr;
        if (!WidgetObj->TryGetObjectField(TEXT("properties"), PropertiesObjectPtr) || !PropertiesObjectPtr || !PropertiesObjectPtr->IsValid())
        {
            return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Each widget entry needs a 'properties' object."));
        }
        PropertiesObj = *PropertiesObjectPtr;

        int32 WidgetKeys = 0;
        for (const auto& PropertyPair : PropertiesObj->Values)
        {
            FString PropertyName = PropertyPair.Key;
            TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
            KeyObj->SetNumberField(TEXT("time"), TimeSeconds);
            KeyObj->SetField(TEXT("value"), PropertyPair.Value);

            TArray<TSharedPtr<FJsonValue>> KeysArray;
            KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));

            TSharedPtr<FJsonObject> SubParams = MakeShared<FJsonObject>();
            SubParams->SetStringField(TEXT("animation_name"), Animation->GetName());
            SubParams->SetStringField(TEXT("widget_name"), WidgetName);
            SubParams->SetStringField(TEXT("property_name"), PropertyName);
            SubParams->SetArrayField(TEXT("keys"), KeysArray);

            if (Params->HasField(TEXT("asset_path")))
            {
                FString AssetPath;
                if (Params->TryGetStringField(TEXT("asset_path"), AssetPath))
                {
                    SubParams->SetStringField(TEXT("asset_path"), AssetPath);
                }
            }

            TSharedPtr<FJsonObject> SubResult = SetPropertyKeys(SubParams);
            if (!SubResult.IsValid() || !SubResult->GetBoolField(TEXT("success")))
            {
                return SubResult.IsValid() ? SubResult : FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Failed to append time slice."));
            }

            ++WidgetKeys;
            ++KeysTotal;
        }

        TSharedPtr<FJsonObject> WidgetSummary = MakeShared<FJsonObject>();
        WidgetSummary->SetStringField(TEXT("widget"), WidgetName);
        WidgetSummary->SetNumberField(TEXT("keys_applied"), WidgetKeys);
        SliceSummaries.Add(MakeShared<FJsonValueObject>(WidgetSummary));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("animation"), Animation->GetName());
    Result->SetNumberField(TEXT("time"), TimeSeconds);
    Result->SetNumberField(TEXT("widgets_updated"), SliceSummaries.Num());
    Result->SetNumberField(TEXT("keys_total"), KeysTotal);
    Result->SetArrayField(TEXT("widgets"), SliceSummaries);
    return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
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

    bool bConfirmed = false;
    Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmed);
    if (!bConfirmed)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Deletion is hardened: set 'confirm_delete': true and prefer animation_delete_widget_keys for scoped removal."));
    }

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
        
        // Refresh Editor
        if (GEditor)
        {
            if (auto* AssetSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
            {
                AssetSub->OpenEditorForAsset(Blueprint);
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("animation"), AnimationName);
        Result->SetStringField(TEXT("widget"), WidgetName);
        Result->SetStringField(TEXT("property"), PropertyName);
        return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
    }
    
    return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Track not found"));
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::RemoveKeys(const TSharedPtr<FJsonObject>& Params)
{
    return DeleteWidgetKeys(Params);
}

TSharedPtr<FJsonObject> FUmgMcpSequencerCommands::DeleteWidgetKeys(const TSharedPtr<FJsonObject>& Params)
{
    bool bConfirmed = false;
    Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmed);
    if (!bConfirmed)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Deletion hardened (Issue 15): set 'confirm_delete': true and target a widget, property, and time."));
    }

    UWidgetBlueprint* Blueprint = nullptr;
    UWidgetAnimation* Animation = nullptr;
    FString Error;
    if (TSharedPtr<FJsonObject> Err = ResolveAnimationContext(Params, Blueprint, Animation, Error)) return Err;

    FString WidgetName;
    if (!ResolveWidgetName(Params, WidgetName))
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' for key deletion."));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName) || PropertyName.IsEmpty())
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' for key deletion."));
    }

    TArray<double> TimesSeconds;
    const TArray<TSharedPtr<FJsonValue>>* TimesArray = nullptr;
    if (Params->TryGetArrayField(TEXT("times"), TimesArray))
    {
        for (const auto& Val : *TimesArray)
        {
            if (Val->Type == EJson::Number) TimesSeconds.Add(Val->AsNumber());
        }
    }
    else
    {
        double SingleTime = 0.0;
        if (Params->TryGetNumberField(TEXT("time"), SingleTime))
        {
            TimesSeconds.Add(SingleTime);
        }
    }

    if (TimesSeconds.Num() == 0)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Provide 'time' or 'times' (seconds) for deletion."));
    }

    UMovieScene* MovieScene = Animation->GetMovieScene();
    if (!MovieScene) return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("MovieScene is null"));
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

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    auto CollectIndices = [&](const TArrayView<const FFrameNumber>& Times) -> TArray<int32>
    {
        TSet<int32> UniqueIndices;
        for (double TimeSeconds : TimesSeconds)
        {
            FFrameNumber TargetFrame = (TimeSeconds * TickResolution).RoundToFrame();
            for (int32 Index = 0; Index < Times.Num(); ++Index)
            {
                if (Times[Index] == TargetFrame)
                {
                    UniqueIndices.Add(Index);
                }
            }
        }
        TArray<int32> Indices = UniqueIndices.Array();
        Algo::Sort(Indices);
        return MoveTemp(Indices);
    };

    bool bTrackFound = false;
    int32 RemovedKeys = 0;

    // Float track
    if (UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneFloatTrack::StaticClass(), WidgetGuid, FName(*PropertyName)))
    {
        bTrackFound = true;
        for (UMovieSceneSection* Section : Track->GetAllSections())
        {
            if (UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section))
            {
                auto Times = FloatSection->GetChannel().GetData().GetTimes();
                TArray<int32> Indices = CollectIndices(Times);
                if (Indices.Num() > 0)
                {
                    TArray<FKeyHandle> Handles;
                    Handles.Reserve(Indices.Num());
                    for (int32 Index : Indices)
                    {
                        Handles.Add(FloatSection->GetChannel().GetHandle(Index));
                    }
                    FloatSection->GetChannel().DeleteKeys(Handles);
                    RemovedKeys += Indices.Num();
                }
            }
        }
    }

    // Color track
    if (UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneColorTrack::StaticClass(), WidgetGuid, FName(*PropertyName)))
    {
        bTrackFound = true;
        for (UMovieSceneSection* Section : Track->GetAllSections())
        {
            if (UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>(Section))
            {
                auto Times = ColorSection->GetRedChannel().GetData().GetTimes();
                TArray<int32> Indices = CollectIndices(Times);
                if (Indices.Num() > 0)
                {
                    TArray<FKeyHandle> Handles;
                    Handles.Reserve(Indices.Num());
                    for (int32 Index : Indices)
                    {
                        Handles.Add(ColorSection->GetRedChannel().GetHandle(Index));
                    }
                    ColorSection->GetRedChannel().DeleteKeys(Handles);
                    ColorSection->GetGreenChannel().DeleteKeys(Handles);
                    ColorSection->GetBlueChannel().DeleteKeys(Handles);
                    ColorSection->GetAlphaChannel().DeleteKeys(Handles);
                    RemovedKeys += Indices.Num();
                }
            }
        }
    }

    // Vector2D track
    if (UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneDoubleVectorTrack::StaticClass(), WidgetGuid, FName(*PropertyName)))
    {
        bTrackFound = true;
        for (UMovieSceneSection* Section : Track->GetAllSections())
        {
            if (UMovieSceneDoubleVectorSection* VectorSection = Cast<UMovieSceneDoubleVectorSection>(Section))
            {
                FMovieSceneChannelProxy& Proxy = VectorSection->GetChannelProxy();
                TArrayView<FMovieSceneDoubleChannel*> Channels = Proxy.GetChannels<FMovieSceneDoubleChannel>();
                if (Channels.Num() >= 2)
                {
                auto Times = Channels[0]->GetData().GetTimes();
                TArray<int32> Indices = CollectIndices(Times);
                if (Indices.Num() > 0)
                    {
                    TArray<FKeyHandle> Handles;
                    Handles.Reserve(Indices.Num());
                    for (int32 Index : Indices)
                    {
                        Handles.Add(Channels[0]->GetHandle(Index));
                    }
                    Channels[0]->DeleteKeys(Handles);
                    Channels[1]->DeleteKeys(Handles);
                        RemovedKeys += Indices.Num();
                    }
                }
            }
        }
    }

    if (!bTrackFound)
    {
        return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Track not found for widget/property."));
    }

    if (RemovedKeys > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        if (GEditor)
        {
            if (auto* AssetSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
            {
                AssetSub->OpenEditorForAsset(Blueprint);
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("animation"), Animation->GetName());
        Result->SetStringField(TEXT("widget"), WidgetName);
        Result->SetStringField(TEXT("property"), PropertyName);
        Result->SetNumberField(TEXT("removed_keys"), RemovedKeys);
        Result->SetNumberField(TEXT("requested_times"), TimesSeconds.Num());
        return FUmgMcpCommonUtils::CreateSuccessResponse(Result);
    }

    return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("No matching keys found at the requested times."));
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
