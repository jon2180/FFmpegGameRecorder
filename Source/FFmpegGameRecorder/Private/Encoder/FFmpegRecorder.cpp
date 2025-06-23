// Fill out your copyright notice in the Description page of Project Settings.


#include "Encoder/FFmpegRecorder.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/App.h"
#include "Widgets/SWindow.h"

#include "Capture/VideoCapture.h"
#include "Encoder/AVEncoder.h"
#include "Encoder/AVEncodeThread.h"

UFFmpegRecorder::UFFmpegRecorder(): RecordConfig()
{
}

UFFmpegRecorder::~UFFmpegRecorder()
{
}

void UFFmpegRecorder::BeginDestroy()
{
	Super::BeginDestroy();
}

void UFFmpegRecorder::Tick(float DeltaTime)
{
}

void UFFmpegRecorder::InitializeRecorderConfig(UWorld* World, FString OutFileName, bool UseGPU, FIntRect& InRect,
                                               int VideoFps, int VideoBitRate, float SoundVolume)
{
	// GameMode = World->WorldType;
	auto AudioDevice = World->GetAudioDevice().GetAudioDevice();

	int32 DeviceAudioSampleRate;
	if (AudioDevice)
	{
		UE_LOG(LogRecorder, Log, TEXT("DeviceSampleRate: %f"), AudioDevice->GetSampleRate())
		DeviceAudioSampleRate = AudioDevice->GetSampleRate();
	}
	else
	{
		DeviceAudioSampleRate = DefaultOutputSampleRate;
	}

	bUseFixedTimeStep = FApp::UseFixedTimeStep();

	// VideoTickTime = static_cast<float>(1) / static_cast<float>(VideoFps);

	auto GameWindow = GEngine->GameViewport->GetWindow().Get();

	// UE_LOG(LogRecorder, Display, TEXT("GEngine->GameViewport->GetWindow().Get()->GetViewportSize() : %s"),
	//     *GameWindow->GetViewportSize().ToString());

	auto width = FormatSize_X(GameWindow->GetViewportSize().X);
	InRect.Max.X = FMath::Min(InRect.Max.X, static_cast<int>(width));
	auto height = FormatSize_X(GameWindow->GetViewportSize().Y);
	InRect.Max.Y = FMath::Min(InRect.Max.Y, static_cast<int>(height));
	RecordConfig.CropArea = InRect;

	RecordConfig.FrameRate = VideoFps;
	RecordConfig.SaveFilePath = OutFileName;
	RecordConfig.SoundVolume = SoundVolume;
	RecordConfig.VideoBitRate = VideoBitRate;
	RecordConfig.bUseHardwareEncoding = UseGPU;
	RecordConfig.AudioSampleRate = DeviceAudioSampleRate;

	RecordConfig.UpdateResolution();
}

void UFFmpegRecorder::InitializeDirector(UWorld* World, FString OutFileName, bool UseGPU, FIntRect InRect, int VideoFps,
                                         int VideoBitRate, float AudioDelay, float SoundVolume)
{
	InitializeRecorderConfig(World, OutFileName, UseGPU, InRect, VideoFps, VideoBitRate, SoundVolume);

	UE_LOG(LogRecorder, Display, TEXT("CaptureRect: %s"), *InRect.ToString());

	// 初始化编码器
	{
		AVBufferedEncoder = MakeShared<FAVBufferedEncoder>();
		AVBufferedEncoder->Initialize();
		AVBufferedEncoder->GetEncoder()->InitializeEncoder(RecordConfig);
	}

	// 初始化编码线程
	{
		Runnable = MakeShared<FAVEncodeThread>(AVBufferedEncoder);
	}

	// 初始化视频获取器
	{
		VideoCapture = MakeShared<FVideoCapture>();
		VideoCapture->Setup();
		VideoCapture->Initialize(RecordConfig.Resolution, RecordConfig.CropArea, RecordConfig.FrameRate);
		VideoCapture->Register(World);
		VideoCapture->GetOnSendFrame().BindRaw(
			AVBufferedEncoder.Get(), &FAVBufferedEncoder::EnqueueVideoFrame_RenderThread);
		VideoCapture->GetOnForceStopRecord().BindUObject(this, &UFFmpegRecorder::StopRecord);
	}

	// 初始化音频监听器
	{
		AudioCapture = MakeShared<FAudioCapture>();
		AudioCapture->Setup();
		AudioCapture->SetAudioFrameSize(AVBufferedEncoder->GetEncoder()->GetAudioFrameSize());
		AudioCapture->RecordConfig = RecordConfig;
		AudioCapture->Register(World);
		AudioCapture->GetOnAudioFrameReadyToSend().BindRaw(
			AVBufferedEncoder.Get(), &FAVBufferedEncoder::EnqueueAudioFrame_AudioThread);
	}

	bRecording = true;
}

void UFFmpegRecorder::StopRecord()
{
	CurrentTime = 0;
	if (!bRecording)
	{
		UE_LOG(LogRecorder, Warning, TEXT("UFFmpegRecorder::StopRecord(): can not stop, is stopping or stopped"));
		return;
	}
	// 防重入
	bRecording = false;

	UE_LOG(LogRecorder, Display, TEXT("UFFmpegRecorder::StopRecord()"));
	if (!Runnable)
	{
		return;
	}

	// 停掉生产线程，停止产出新数据，只剩消费线程
	VideoCapture->Unregister();
	AudioCapture->Unregister();
	UE_LOG(LogRecorder, Display, TEXT("unregistered all delegate (receiver)"));

	// 等待消费线程继续处理
	Runnable->Stop();
	// 线程会很快结束并退出
	Runnable->Kill();
	UE_LOG(LogRecorder, Display, TEXT("Encoder thread exited."));

	// 消费线程处理完了，正常退出，几个线程都执行完了可以删除编码线程了
	Runnable.Reset();
}
