#pragma once

#include "CoreMinimal.h"
#include "AVRecorderBase.h"
#include "Engine/EngineTypes.h"

// #if ENGINE_MAJOR_VERSION >= 5
// #include "ISubmixBufferListener.h"
// #else
#include "AudioDevice.h"
#include "RecorderConfig.h"
// #endif


class FAudioDevice;
DECLARE_DELEGATE_SevenParams(FOnAudioFrameReadyToSend, float* AudioData, int NumSamples, int32 NumChannels,
                             int32 SampleRate, double AudioClock, double PresentTime, double Duration)

/**
 * 音频录制器，负责监听音频，然后按固定的大小发给外部
 */
class FFMPEGGAMERECORDER_API FAudioCapture final : public IAVRecorderBase, public ISubmixBufferListener
{
public:
	FAudioCapture();
	virtual ~FAudioCapture() override;

	/** ISubmixBufferListener Implementations */
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples,
	                               int32 NumChannels,
	                               const int32 SampleRate, double AudioClock) override;

	/** IAVRecorderBase Implementations */
	virtual void Register(UWorld* World) override;
	virtual void Unregister() override;

	FOnAudioFrameReadyToSend& GetOnAudioFrameReadyToSend()
	{
		return OnAudioFrameReadyToSend;
	}

	void SetAudioFrameSize(const int32 Size) { MaxAllowFrame = Size; }

public:
	// 配置
	FRecorderConfig RecordConfig;

protected:
	FCriticalSection SendCS;
	FOnAudioFrameReadyToSend OnAudioFrameReadyToSend;

	FAudioDevice* AudioDevice;
	EWorldType::Type WorldType;

	/** 来自编码器接收的最大的采样数 */
	int32 MaxAllowFrame;

	/** Buffers for batch send audio raw data, 两倍 Audio frame_size 长度，用来使每次音频传入的数据都是 frame_size 大小 */
	TArray<float> AudioSendBuffer;

	/** 简易累加器，每次音频的时间长度是一致的 */
	double AudioFrameTime = 0;
};
