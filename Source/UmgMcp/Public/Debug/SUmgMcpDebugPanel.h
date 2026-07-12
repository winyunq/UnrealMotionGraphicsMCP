// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SMultiLineEditableTextBox;
class STextBlock;

/** Protocol console backed by UUmgMcpBridge's real request pipeline. */
class UMGMCP_API SUmgMcpDebugPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SUmgMcpDebugPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
    FReply ExecuteInput();
    FReply InsertPing();
    FReply InsertConnect();
    void RefreshTrace();

    TSharedPtr<SMultiLineEditableTextBox> RequestEditor;
    TSharedPtr<SMultiLineEditableTextBox> ResponseViewer;
    TSharedPtr<SMultiLineEditableTextBox> TraceViewer;
    TSharedPtr<STextBlock> ServerStatus;
    double LastRefreshAt = 0.0;
};
