// Fill out your copyright notice in the Description page of Project Settings.


#include "GameRecorderEntry.h"

#include "Capture/AudioCapture.h"
#include "Encoder/AVEncodeThread.h"
#include "Encoder/FFmpegRecorder.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"

TWeakObjectPtr<UFFmpegRecorder> UGameRecorderEntry::CurrentDirector(nullptr);


UWorld* UGameRecorderEntry::GetWorldContext(UObject* WorldContextObject)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
    return World;
}

FString UGameRecorderEntry::GetSavePath()
{
    FString SaveDirectory = FPaths::ProjectSavedDir();
#if PLATFORM_ANDROID
    extern FString GFilePathBase;
    UE_LOG(LogTemp, Log, TEXT("Before FFmpegFunctionLibrary::GetSavePath = %s"), *SaveDirectory);
    UE_LOG(LogTemp, Log, TEXT("Before FFmpegFunctionLibrary::GetSavePath = %s"), *FPaths::ConvertRelativePathToFull(SaveDirectory));
    if (UserAndroidAbsolutePath)
    {
        SaveDirectory = GFilePathBase + FString("/UE4Game/") + FApp::GetProjectName() + FString(TEXT("/")) + FApp::GetProjectName() + TEXT("/Saved");
    }
#endif

    UE_LOG(LogTemp, Log, TEXT("FFmpegFunctionLibrary::GetSavePath = %s"), *SaveDirectory);
    return SaveDirectory + TEXT("/VideoCaptures/");
}

FString UGameRecorderEntry::StartRecord(int ScreenX, int ScreenY, int ScreenW, int ScreenH)
{
    if (CurrentDirector.IsValid())
    {
        UE_LOG(LogRecorder, Warning, TEXT("Stop last record"))
        StopRecord();
    }

    if (bUseFixedTimeStepRecording)
    {
        PreviousFixedDeltaTime = FApp::GetFixedDeltaTime();
        FApp::SetFixedDeltaTime(1 / VideoFrameRate);
        FApp::SetUseFixedTimeStep(true);
    }

    FIntRect InRect(ScreenX, ScreenY, ScreenW + ScreenX, ScreenH + ScreenY);

    UE_LOG(LogRecorder, Display, TEXT("ScreenX: %d, ScreenY: %d, ScreenW: %d, ScreenH: %d"), ScreenX, ScreenY, ScreenW, ScreenH);

    auto RawCurrentDirector = NewObject<UFFmpegRecorder>();
    if (!IsValid(RawCurrentDirector))
    {
        return FString();
    }
    RawCurrentDirector->AddToRoot();

    FVector2D Size;
    GEngine->GameViewport->GetViewportSize(Size);

    UE_LOG(LogRecorder, Display, TEXT("GEngine->GameViewport->GetViewportSize : %s"), *Size.ToString());
    FString SaveDirectory = GetSavePath();
    if (!IFileManager::Get().DirectoryExists(*SaveDirectory))
    {
        IFileManager::Get().MakeDirectory(*SaveDirectory);
    }

    // #if PLATFORM_MAC || PLATFORM_WINDOWS
    FDateTime Now = FDateTime::Now();
    const FString OutFileName = SaveDirectory + FString::Printf(TEXT("GameRecord-%s.mp4"), *Now.ToString());
    // #else
    //     const FString OutFileName = SaveDirectory + "MyGame.mp4";
    // #endif

    //FString VideoFilter = "scale=";
    //VideoFilter += FString::SanitizeFloat(w) + ":" + FString::SanitizeFloat(h);
    constexpr int32 VideoBitRate = 12 * 1024 * 1024 /*80000000*/; // 12 Mbps
    RawCurrentDirector->InitializeDirector(GEngine->GameViewport->GetWorld(), OutFileName, false, InRect, VideoFrameRate,
        VideoBitRate, 0.01, 0.5);
    CurrentDirector = RawCurrentDirector;
    UE_LOG(LogRecorder, Log, TEXT("Mp4 Save to = GetSavePath = %s"), *OutFileName);
    return OutFileName;
}

void UGameRecorderEntry::CaptureNextFrame()
{
}

FString UGameRecorderEntry::ScreenShot(int ScreenX, int ScreenY, int ScreenW, int ScreenH)
{
    return FString();
}

void UGameRecorderEntry::StopRecord()
{
    UE_LOG(LogRecorder, Display, TEXT("UFFmpegFunctionLibrary::StopRecord(): stop record CurrentDirectory is %d"),
    CurrentDirector != nullptr)
    if (CurrentDirector.IsValid())
    {
        // CurrentDirector->EndWindowReader(true);
        CurrentDirector->StopRecord();
        CurrentDirector->RemoveFromRoot();
        // CurrentDirector->MarkAsGarbage();
        CurrentDirector->MarkPendingKill();
        CurrentDirector = nullptr;
    }
    if (bUseFixedTimeStepRecording)
    {
        FApp::SetFixedDeltaTime(PreviousFixedDeltaTime);
        FApp::SetUseFixedTimeStep(false);
    }
}