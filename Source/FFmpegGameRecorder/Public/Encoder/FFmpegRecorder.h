// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Tickable.h"

#include "Capture/AudioCapture.h"
#include "Capture/RecorderConfig.h"

#include "FFmpegRecorder.generated.h"

class FAVBufferedEncoder;
class FVideoCapture;
class FAVEncodeThread;
class FAVEncoder;
/**
 *
 */
UCLASS()
class FFMPEGGAMERECORDER_API UFFmpegRecorder : public UObject, public FTickableGameObject
{
    GENERATED_BODY()

public:
    //~UObject interface
    UFFmpegRecorder();
    virtual ~UFFmpegRecorder() override;
    virtual void BeginDestroy() override;
    //~UObject interface

    //~FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;

    FORCEINLINE virtual bool IsTickable() const override
    {
        return bUseFixedTimeStep;
    }

    FORCEINLINE virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UTickableObject, STATGROUP_Tickables);
    }

    //~FTickableGameObject interface

    double CurrentTime = 0;

    // bool IsCurrentAsHost() const;

    void InitializeRecorderConfig(UWorld* World, FString OutFileName, bool UseGPU, FIntRect& InRect, int VideoFps,
        int VideoBitRate, float SoundVolume);

    void InitializeDirector(UWorld* World, FString OutFileName, bool UseGPU, FIntRect InRect, int VideoFps,
        int VideoBitRate,
        float AudioDelay, float SoundVolume);

    void StopRecord();

    // 配置
    FRecorderConfig RecordConfig;
    bool bUseFixedTimeStep = false;
    std::atomic_bool bRecording{false};


    // TSharedPtr<FAVEncoder> AVEncoder;
    TSharedPtr<FAVBufferedEncoder> AVBufferedEncoder;
    TSharedPtr<FVideoCapture> VideoCapture;
    TSharedPtr<FAudioCapture> AudioCapture;
    // 运行时需求
    TSharedPtr<FAVEncodeThread> Runnable;
};