#include "Capture/VideoCapture.h"

#include "Capture/RecorderConfig.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Stats/StatsMisc.h"

void FScreenCaptureTimeManager::Initialize(double InOutputFrameRate)
{
	OutputFrameInterval = InOutputFrameRate;
	ExpectedOutputFrameInterval = InOutputFrameRate;
	LastOutputTimestamp = 0.0;
	InputTimeAccumulator = 0.0;
}

bool FScreenCaptureTimeManager::ShouldProcessThisFrame(double InInputTime)
{
	// 仅初始化
	if (RecordStartVideoTimeClock <= 0)
	{
		RecordStartVideoTimeClock = InInputTime;
		LastOutputTimestamp = 0.0;
		InputTimeAccumulator = 0.0;
		return true;
	}

	// 当前时间
	const double CurrentTimestamp = (InInputTime - RecordStartVideoTimeClock);
	// 当前帧到达时间已经大于预设帧率的间隔时间，直接判定为接收
	if (LastOutputTimestamp + ExpectedOutputFrameInterval <= CurrentTimestamp)
	{
		// TODO 这里的代码会重置时间戳，这会导致输出视频的帧率不合预期，需要重置
		InputTimeAccumulator = 0;
		OutputFrameInterval = CurrentTimestamp - LastOutputTimestamp;
		LastOutputTimestamp = CurrentTimestamp;
		UE_LOG(LogRecorder, Display, TEXT("ShouldProcessThisFrame: %s"), *ToString())
		return true;
	}
	else
	{
		// 强制重置帧时长
		{
			OutputFrameInterval = ExpectedOutputFrameInterval;
		}
		// 时基累积算法
		InputTimeAccumulator += CurrentTimestamp - LastOutputTimestamp;

		// 累加器判定
		constexpr double TIME_SPAN_TOLERANCE = 0.10;
		constexpr double DeltaTolerance = 1 - TIME_SPAN_TOLERANCE;
		if (InputTimeAccumulator < OutputFrameInterval * DeltaTolerance)
		{
			return false;
		}
		InputTimeAccumulator -= OutputFrameInterval;

		LastOutputTimestamp += OutputFrameInterval;
		return true;
	}
}

FString FScreenCaptureTimeManager::ToString() const
{
	return FString::Printf(TEXT(
		"{ExpectedOutputFrameInterval=%lf, RecordStartVideoTimeClock=%lf, OutputFrameInterval=%lf, LastOutputTimestamp=%lf, InputTimeAccumulator=%lf}"),
	                       ExpectedOutputFrameInterval, RecordStartVideoTimeClock, OutputFrameInterval,
	                       LastOutputTimestamp, InputTimeAccumulator);
}

FVideoCapture::FVideoCapture(): VideoTickTime(0), GameWindow(nullptr)
{
}

FVideoCapture::~FVideoCapture()
{
}

void FVideoCapture::Register(UWorld* World)
{
	if (!bInitialized)
	{
		UE_LOG(LogRecorder, Error, TEXT("FVideoCapture::Register: cant register before initialization"));
		return;
	}

	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(
		this, &FVideoCapture::OnBackBufferReady_RenderThread);
	// 使用 PreResize，而不是 PostResize，为了保证截取的部分是有效的
	FSlateApplication::Get().GetRenderer()->OnPreResizeWindowBackBuffer().AddRaw(
		this, &FVideoCapture::OnPreResizeWindowBackBuffer);
	FSlateApplication::Get().GetRenderer()->OnSlateWindowDestroyed().AddRaw(
		this, &FVideoCapture::EndWindowReader_StandardGame);
	GameWindow = GEngine->GameViewport->GetWindow().Get();
	bRecording.store(true);
}

void FVideoCapture::Unregister()
{
	bRecording.store(false);
	{
		FScopeLock Lock(&VideoCaptureCS);
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);
		FSlateApplication::Get().GetRenderer()->OnPreResizeWindowBackBuffer().RemoveAll(this);
		FSlateApplication::Get().GetRenderer()->OnSlateWindowDestroyed().RemoveAll(this);
		GetOnSendFrame().Unbind();
		GetOnForceStopRecord().Unbind();
	}
	GameWindow = nullptr;
	UE_LOG(LogRecorder, Display, TEXT("Frame receiver stopped."));
}

bool FVideoCapture::CopyTextureToQueue_GpuReadToCpu(const FTexture2DRHIRef& BackBuffer, double CaptureTsInSeconds,
                                                    double DurationInSeconds, const FIntRect& RecordArea)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("CopyTextureToQueue_GpuReadToCpu");

	int w = BackBuffer->GetTexture2D()->GetSizeX();
	int h = BackBuffer->GetTexture2D()->GetSizeY();

	FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();

	// enqueue 到队列中使写入画面
	{
		if (!GPUTextureReadback[CurrentReadbackIndex].IsValid())
		{
			static FName RecordName = TEXT("ScreenRecordTextureReadback");
			GPUTextureReadback[CurrentReadbackIndex].Reset(
				new recorder::FRHIGPUTextureReadback(RecordName, BackBuffer->GetTexture2D()->GetSizeXY()));
		}
		auto& CurrentGpuReadBack = GPUTextureReadback[CurrentReadbackIndex];

		{
			// FScopeLogTime timecr(TEXT("EnqueueCopyRDG"));
			// FResolveRect Rect{RecordArea};
			FResolveRect Rect{};
			CurrentGpuReadBack->PreEnqueue(CaptureTsInSeconds, DurationInSeconds);
			CurrentGpuReadBack->EnqueueCopy(RHICmdList, BackBuffer->GetTexture2D(), Rect);
		}
	}

	// READBACK_BUFFER_COUNT - 1 帧前的画面，用于交替读取
	const int32 PreviousReadbackIndex = (CurrentReadbackIndex + 1) % READBACK_BUFFER_COUNT;
	CurrentReadbackIndex = PreviousReadbackIndex;

	// 验证用于读取的是否准备好了，没准备好说明还在初始化，先不读
	if (!GPUTextureReadback[PreviousReadbackIndex].IsValid())
	{
		static FName RecordName = TEXT("ScreenRecordTextureReadback");
		GPUTextureReadback[PreviousReadbackIndex].Reset(
			new recorder::FRHIGPUTextureReadback(RecordName, BackBuffer->GetTexture2D()->GetSizeXY()));
		return true;
	}

	// 下面肯定已经保证了前一帧的 readback 有效
	bool Rst = false;
	{
		auto& PreviousGpuReadback = GPUTextureReadback[PreviousReadbackIndex];

		if (
			// 这个怎么在 Android 上会始终为 false 啊？
#if PLATFORM_IOS || PLATFORM_MAC || PLATFORM_WINDOWS
			!PreviousGpuReadback->IsReady() ||
#endif
			PreviousGpuReadback->GetCaptureStatus() != recorder::FRHIGPUTextureReadback::ECaptureStatus::Capturing)
		{
			UE_LOG(LogRecorder, Error, TEXT("GPUReadToCpu: copy texture is not ready: isReady=%d, CaptureStatus=%d"),
			       PreviousGpuReadback->IsReady(), PreviousGpuReadback->GetCaptureStatus())
			return false;
		}

		uint8* TextureData;
		FIntPoint OutResolution;
		{
			// FScopeLogTime timecr(TEXT("Lock/Unlock"));
			void* VoidTextureData = nullptr;
			PreviousGpuReadback->LockTexture(RHICmdList, VoidTextureData, OutResolution);
			TextureData = static_cast<uint8*>(VoidTextureData);
		}

		UE_LOG(LogRecorder, Verbose,
		       TEXT("SourceResolution: (Width=%d, Height=%d) RecordArea: %s"), w, h, *RecordArea.ToString())

		// 强制截取的视频帧包含的画面宽高有效，主要针对 Resize 场景做安全保证
		auto ClippedRect = RecordArea;
		ClippedRect.Max.ComponentMin(FIntPoint(OutResolution.X, h));
		{
			Rst = GetOnSendFrame().ExecuteIfBound(FCapturedVideoFrame{
				.FrameData =  TextureData,
				.PixelFormat = BackBuffer->GetFormat(),
				.FrameWidth = static_cast<uint16>(OutResolution.X),
				.FrameHeight = static_cast<uint16>(h),
				.CaptureRect = ClippedRect,
				.PresentTime = PreviousGpuReadback->CapturedTime,
				.Duration = PreviousGpuReadback->CapturedDuration
			});
		}
		PreviousGpuReadback->Unlock();
	}

	return Rst;
}

bool FVideoCapture::CopyTextureToQueue_LockTextureToCpu(const FTexture2DRHIRef& BackBuffer, double CaptureTsInSeconds,
                                                        double DurationInSeconds, const FIntRect& RecordArea)
{
	FRHICommandListImmediate& list = GRHICommandList.GetImmediateCommandList();
	const int w = BackBuffer->GetTexture2D()->GetSizeX();
	const int h = BackBuffer->GetTexture2D()->GetSizeY();

	FIntPoint OutResolution(w, h);
	uint8* TextureData;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("LockTexture2D");
		uint32 TextureRowStrip;
		TextureData = (uint8*)list.LockTexture2D(BackBuffer->GetTexture2D(),
		                                         0, EResourceLockMode::RLM_ReadOnly, TextureRowStrip, false);
		int FrameWidth = TextureRowStrip / 4;
		if (RecordArea.Width() != FrameWidth && FrameWidth != 0)
		{
			OutResolution.X = FrameWidth;
		}
	}

	bool Rst = false;
	// 强制截取的视频帧包含的画面宽高有效，主要针对 Resize 场景做安全保证
	auto ClippedRect = RecordArea;
	ClippedRect.Max.ComponentMin(FIntPoint(OutResolution.X, h));
	{
		Rst = GetOnSendFrame().ExecuteIfBound(FCapturedVideoFrame{
			.FrameData = TextureData,
			.PixelFormat = BackBuffer->GetFormat(),
			.FrameWidth = static_cast<uint16>(OutResolution.X),
			.FrameHeight = static_cast<uint16>(h),
			.CaptureRect = ClippedRect,
			.PresentTime = CaptureTsInSeconds,
			.Duration = DurationInSeconds
		});
	}
	list.UnlockTexture2D(BackBuffer, 0, false);
	return Rst;
}

void FVideoCapture::OnPreResizeWindowBackBuffer(void* BackBuffer)
{
	// BackBuffer 参数对实际应用无效
	StopRecord();
}

/** 切记等待和复杂操作 */
void FVideoCapture::OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer)
{
	FScopeLock ScopeLock(&VideoCaptureCS);
	if (!bRecording)
	{
		return;
	}
	if (GameWindow != &SlateWindow)
	{
		return;
	}
	ensure(IsInRenderingThread());

	double CurrentGameTime = FApp::GetCurrentTime();
	if (TimeManager.ShouldProcessThisFrame(CurrentGameTime))
	{
		bool RecordRst;
		if (CVarRecordFrameRemapEnabled->GetBool())
		{
			RecordRst = CopyTextureToQueue_GpuReadToCpu(
				BackBuffer, TimeManager.GetNextOutputTimestamp(), TimeManager.GetOutputDuration(), CropArea);
		}
		else
		{
			RecordRst = CopyTextureToQueue_LockTextureToCpu(
				BackBuffer, TimeManager.GetNextOutputTimestamp(), TimeManager.GetOutputDuration(), CropArea);
		}
		if (!RecordRst)
		{
			UE_LOG(LogRecorder, Error, TEXT("failed to send frame"))
		}
	}
}

void FVideoCapture::StopRecord()
{
	// 避免重复停止，避免死循环
	if (bRecording.load())
	{
		bRecording.store(false);
		OnForceStopRecord.ExecuteIfBound();
	}
}


void FVideoCapture::EndWindowReader(const bool i)
{
	StopRecord();
}

void FVideoCapture::EndWindowReader_StandardGame(void* i)
{
	StopRecord();
}
