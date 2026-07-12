// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Debug/SUmgMcpDebugPanel.h"

#include "Bridge/UmgMcpBridge.h"
#include "Editor.h"
#include "HAL/PlatformTime.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUmgMcpDebugPanel"

void SUmgMcpDebugPanel::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)
            [
                SAssignNew(ServerStatus, STextBlock)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
            [
                SNew(SButton).Text(LOCTEXT("ConnectTemplate", "连接模板"))
                .OnClicked(this, &SUmgMcpDebugPanel::InsertConnect)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
            [
                SNew(SButton).Text(LOCTEXT("PingTemplate", "Ping 模板"))
                .OnClicked(this, &SUmgMcpDebugPanel::InsertPing)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton).Text(LOCTEXT("Execute", "按真实管线执行"))
                .OnClicked(this, &SUmgMcpDebugPanel::ExecuteInput)
            ]
        ]
        + SVerticalBox::Slot().FillHeight(0.42f).Padding(6, 0, 6, 3)
        [
            SNew(SSplitter)
            + SSplitter::Slot().Value(0.5f)
            [
                SNew(SBorder)
                [
                    SAssignNew(RequestEditor, SMultiLineEditableTextBox)
                    .Text(FText::FromString(TEXT("{\n  \"command\": \"connect\",\n  \"client_id\": \"debug-ui\",\n  \"params\": {\"display_name\": \"Debug UI\", \"exclusive\": true}\n}")))
                ]
            ]
            + SSplitter::Slot().Value(0.5f)
            [
                SNew(SBorder)
                [
                    SAssignNew(ResponseViewer, SMultiLineEditableTextBox)
                    .IsReadOnly(true)
                ]
            ]
        ]
        + SVerticalBox::Slot().FillHeight(0.58f).Padding(6, 3, 6, 6)
        [
            SNew(SBorder)
            [
                SAssignNew(TraceViewer, SMultiLineEditableTextBox)
                .IsReadOnly(true)
            ]
        ]
    ];
    RefreshTrace();
}

void SUmgMcpDebugPanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (InCurrentTime - LastRefreshAt >= 0.25)
    {
        LastRefreshAt = InCurrentTime;
        RefreshTrace();
    }
}

FReply SUmgMcpDebugPanel::ExecuteInput()
{
    if (GEditor && RequestEditor.IsValid())
    {
        if (UUmgMcpBridge* Bridge = GEditor->GetEditorSubsystem<UUmgMcpBridge>())
        {
            ResponseViewer->SetText(FText::FromString(Bridge->ExecuteDebugMessage(RequestEditor->GetText().ToString())));
            RefreshTrace();
        }
    }
    return FReply::Handled();
}

FReply SUmgMcpDebugPanel::InsertPing()
{
    RequestEditor->SetText(FText::FromString(TEXT("{\n  \"command\": \"ping\",\n  \"client_id\": \"debug-ui\",\n  \"request_id\": \"manual-ping\",\n  \"params\": {}\n}")));
    return FReply::Handled();
}

FReply SUmgMcpDebugPanel::InsertConnect()
{
    RequestEditor->SetText(FText::FromString(TEXT("{\n  \"command\": \"connect\",\n  \"client_id\": \"debug-ui\",\n  \"params\": {\"display_name\": \"Debug UI\", \"exclusive\": true}\n}")));
    return FReply::Handled();
}

void SUmgMcpDebugPanel::RefreshTrace()
{
    if (!GEditor || !TraceViewer.IsValid()) return;
    UUmgMcpBridge* Bridge = GEditor->GetEditorSubsystem<UUmgMcpBridge>();
    if (!Bridge) return;

    ServerStatus->SetText(FText::Format(LOCTEXT("ServerStatus", "实例 {0}  |  127.0.0.1:{1}  |  所有请求 FIFO 串行执行"),
        FText::FromString(Bridge->GetServerInstanceId()), FText::AsNumber(Bridge->GetListeningPort())));
    TArray<FMcpDebugRecord> Records;
    Bridge->GetDebugRecords(Records);
    FString Trace;
    for (const FMcpDebugRecord& Record : Records)
    {
        Trace += FString::Printf(TEXT("#%llu  %s  [%s]  client=%s  request=%s  command=%s  %.2f ms\n> %s\n< %s\n\n"),
            Record.Sequence, *Record.Time, *Record.State, *Record.ClientId, *Record.RequestId,
            *Record.Command, Record.DurationMs, *Record.RequestJson, *Record.ResponseJson);
    }
    TraceViewer->SetText(FText::FromString(Trace));
}

#undef LOCTEXT_NAMESPACE
