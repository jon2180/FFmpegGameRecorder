#include "Capture/AudioCapture.h"

#include "AudioDevice.h"
#include "Engine/World.h"

FAudioCapture::FAudioCapture()
	: RecordConfig()
	  , AudioDevice(nullptr)
	  , WorldType()
	  , MaxAllowFrame(0)
{
}

FAudioCapture::~FAudioCapture()
{
}

void FAudioCapture::OnNewSubmixBuffer(
	const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples,
	int32 NumChannels, const int32 SampleRate, double AudioClock)
{
	UE_LOG(LogRecorder, Verbose,
	       TEXT("OnNewSubmixBuffer: NumSamples=%d, NumChannels=%d, SampleRate=%d, AudioClock=%lf"),
	       NumSamples, NumChannels, SampleRate, AudioClock)

	AudioSendBuffer.Append(AudioData, NumSamples);

	// 分包发送, 因为 Encoder 有帧大小限制
	const int32 BufferFullSize = MaxAllowFrame * NumChannels;
	const double DurationSecond = static_cast<double>(MaxAllowFrame) / SampleRate;
	while (AudioSendBuffer.Num() >= BufferFullSize)
	{
		bool bSent;
		{
			FScopeLock Lock(&SendCS);
			bSent = GetOnAudioFrameReadyToSend().ExecuteIfBound(
				AudioSendBuffer.GetData(), MaxAllowFrame, NumChannels, SampleRate, AudioClock,
				AudioFrameTime, DurationSecond);
		}

		if (bSent)
		{
			AudioFrameTime += DurationSecond;
#if 0
			AudioSendBuffer.RemoveAt(0, BufferFullSize, EAllowShrinking::No);
#else
			AudioSendBuffer.RemoveAt(0, BufferFullSize, false);
#endif
		}
		else
		{
			// UE_LOG(LogTemp, Error, TEXT("DEBUG_AUDIO an audio frame maybe discard CurrentTimeSeq: %lf FrameCount %d"), NewAudioClock, consume_frame);
		}
	}
}

void FAudioCapture::Register(UWorld* World)
{
	if (!World)
	{
		UE_LOG(LogRecorder, Warning, TEXT("World is invalid"))
		return;
	}

	WorldType = World->WorldType;
	AudioDevice = World->GetAudioDevice().GetAudioDevice();
	// AudioDevice = FAudioDeviceManager::Get()->GetActiveAudioDevice().GetAudioDevice();
	if (AudioDevice)
	{
		AudioDevice->RegisterSubmixBufferListener(this/*AsShared()*/ /*, AudioDevice->GetMainSubmixObject()*/);
	}
}

void FAudioCapture::Unregister()
{
	// 音效线程执行完了吗，
	// FScopeLock ScopeLock2(&AudioThreadOK);

	if (AudioDevice)
	{
		{
			FScopeLock Lock(&SendCS);
			AudioDevice->UnregisterSubmixBufferListener(this/*, AsShared()*//*, AudioDevice->GetMainSubmixObject()*/);
		}
		UE_LOG(LogRecorder, Display, TEXT("Audio receiver stopped."))
	}
}
