#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

#include "RecorderConfig.generated.h"

USTRUCT(BlueprintType)
struct FRecorderConfig
{
	GENERATED_BODY()

	UPROPERTY()
	FString SaveFilePath;

	/** 帧率 */
	UPROPERTY()
	int32 FrameRate;

	/** 截取分辨率 */
	// UPROPERTY()
	FIntRect CropArea;

	/** 是否使用硬件编码 */
	UPROPERTY()
	bool bUseHardwareEncoding;

	UPROPERTY()
	int32 VideoBitRate;

	UPROPERTY()
	int32 AudioBitRate;

	UPROPERTY()
	int32 AudioSampleRate;

	UPROPERTY()
	float SoundVolume;

	/** 分辨率，不需要传入 */
	UPROPERTY()
	FIntPoint Resolution;

	void UpdateResolution()
	{
		Resolution = CropArea.Size();
	}
};

class FEncodeData
{
public:
	FEncodeData();
	~FEncodeData();

	FORCEINLINE const uint8_t* GetRawData() const
	{
		return reinterpret_cast<const uint8_t*>(Data.GetData());
	}

	FORCEINLINE uint8_t* GetRawData()
	{
		return reinterpret_cast<uint8_t*>(Data.GetData());
	}

	FORCEINLINE SIZE_T GetRawLength() const
	{
		return Data.Num() * sizeof(float);
	}

	void Initialize(uint32 Size);

	FCriticalSection ModifyCS;
	TArray<float> Data;
	double StartSec;
	double Duration;

private:
	// 禁用复制
	FEncodeData(const FEncodeData& Other) = delete;
	FEncodeData(FEncodeData&& Other) = delete;
	FEncodeData& operator=(const FEncodeData& Other) = delete;
	FEncodeData& operator=(FEncodeData&& Other) = delete;
};


static FAutoConsoleVariable CVarRecordFrameRemapEnabled(
	TEXT("r.recorder.FrameRemapEnabled"),
#if PLATFORM_ANDROID
    0,
#else
	1,
#endif
	TEXT("remap back buffer to another readback buffer which FrameRemapEnabled(value equals 1).\n"),
	ECVF_Default);

static int32 ConstantRateFactor = 18;
static FAutoConsoleVariableRef CVarConstantRateFactor(
	TEXT("rec.crf"), ConstantRateFactor,
	TEXT("Constant Rate Factor, higher means smaller videos and lower quality, range: 0 ~ 51, recommend: 18 ~ 28"),
	ECVF_Default);


constexpr int32 DefaultOutputSampleRate = 48000;

static int32 UserAndroidAbsolutePath = 1;
static bool bUseFixedTimeStepRecording = false;
static float PreviousFixedDeltaTime = 0;
static float VideoFrameRate = 24.f;

static FAutoConsoleVariableRef CVarUserAndroidAbsolutePath(
	TEXT("MW.UserAndroidAbsolutePath"), UserAndroidAbsolutePath, TEXT("Use android absolutepath"), ECVF_Default);
static FAutoConsoleVariableRef CVarUseFixedTimeStepRecording(
	TEXT("rec.UseFixedTimeStep"), bUseFixedTimeStepRecording,
	TEXT(
		"Use the fixed time step to run the game when recording video. Fixed delta time will be set to 1 / VideoFrameRate. May be useful on low-end platforms."));
static FAutoConsoleVariableRef CVarVideoFrameRate(TEXT("rec.VideoFrameRate"), VideoFrameRate,
                                                  TEXT("Out put video frame rate"));
