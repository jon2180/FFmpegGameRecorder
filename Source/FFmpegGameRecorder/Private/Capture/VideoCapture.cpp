#include "Capture/VideoCapture.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Stats/StatsMisc.h"

namespace recorder
{
	FRHIGPUTextureReadback::FRHIGPUTextureReadback(FName RequestName, FIntPoint Resolution): Resolution(Resolution)
		, CapturedTime(0)
		, CapturedDuration(0)
		, CaptureStatus(ECaptureStatus::Idle)
	{
		Fence = RHICreateGPUFence(RequestName);
	}

	FRHIGPUTextureReadback::~FRHIGPUTextureReadback()
	{
		if (DestinationStagingTexture)
		{
			DestinationStagingTexture.SafeRelease();
		}
	}

	void FRHIGPUTextureReadback::EnqueueCopyRDG(FRHICommandList& RHICmdList, FRHITexture* SourceTexture,
	                                            FResolveRect Rect)
	{
		// SourceTexture is already in CopySrc state (handled by RDG)
		EnqueueCopyInternal(RHICmdList, SourceTexture, FResolveParams(Rect));
	}

	void FRHIGPUTextureReadback::EnqueueCopy(FRHICommandList& RHICmdList, FRHITexture* SourceTexture, FResolveRect Rect)
	{
		// In the non-RDG version, we don't know what state the source texture will already be in, so transition it to CopySrc.
		RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
		EnqueueCopyInternal(RHICmdList, SourceTexture, FResolveParams(Rect));
	}

	void FRHIGPUTextureReadback::EnqueueCopyInternal(FRHICommandList& RHICmdList, FRHITexture* SourceTexture,
	                                                 FResolveParams ResolveParams)
	{
		Fence->Clear();

		if (SourceTexture)
		{
			// We only support 2d textures for now.
			ensure(SourceTexture->GetTexture2D());

			// Assume for now that every enqueue happens on a texture of the same format and size (when reused).
			if (!DestinationStagingTexture)
			{
				FIntVector TextureSize;
				// if (ResolveParams.Rect.IsValid())
				// {
				//     FIntPoint SourceTexture2D(SourceTexture->GetSizeXYZ().X, SourceTexture->GetSizeXYZ().Y);
				//     FIntPoint TargetRect(ResolveParams.Rect.X2 - ResolveParams.Rect.X1, ResolveParams.Rect.Y2 - ResolveParams.Rect.Y1);
				//     FIntPoint RstRect = SourceTexture2D.ComponentMin(TargetRect);
				//     TextureSize = FIntVector(RstRect.X, RstRect.Y, 0);
				// }
				// else
				// {
				TextureSize = SourceTexture->GetSizeXYZ();
				// }

				FString FenceName = Fence->GetFName().ToString();
				FRHIResourceCreateInfo CreateInfo(*FenceName);
				DestinationStagingTexture = RHICreateTexture2D(TextureSize.X, TextureSize.Y, SourceTexture->GetFormat(),
				                                               1, 1,
				                                               TexCreate_CPUReadback | TexCreate_HideInVisualizeTexture,
				                                               CreateInfo);
			}

			// We need the destination texture to be writable from a copy operation
			RHICmdList.Transition(FRHITransitionInfo(DestinationStagingTexture, ERHIAccess::Unknown,
			                                         ERHIAccess::CopyDest));

			// Ensure this copy call does not perform any transitions. We're handling them manually.
			ResolveParams.SourceAccessFinal = ERHIAccess::Unknown;
			ResolveParams.DestAccessFinal = ERHIAccess::Unknown;

			// Transfer memory GPU -> CPU
			RHICmdList.CopyToResolveTarget(SourceTexture, DestinationStagingTexture, ResolveParams);

			// Transition the dest to CPURead *before* signaling the fence, otherwise ordering is not guaranteed.
			RHICmdList.Transition(FRHITransitionInfo(DestinationStagingTexture, ERHIAccess::CopyDest,
			                                         ERHIAccess::CPURead));
			RHICmdList.WriteGPUFence(Fence);

			LastCopyGPUMask = RHICmdList.GetGPUMask();

			// --------------- start custom
			CaptureStatus = ECaptureStatus::Capturing;
			// --------------- end custom
		}
	}

	void FRHIGPUTextureReadback::LockTexture(FRHICommandListImmediate& RHICmdList, void*& OutBufferPtr,
	                                         FIntPoint& OutRowPitchInPixels)
	{
		if (DestinationStagingTexture)
		{
			void* ResultsBuffer = nullptr;
			RHICmdList.MapStagingSurface(DestinationStagingTexture, Fence.GetReference(), ResultsBuffer,
			                             OutRowPitchInPixels.X, OutRowPitchInPixels.Y);
			OutBufferPtr = ResultsBuffer;
			CaptureStatus = ECaptureStatus::Captured;
		}
		else
		{
			OutBufferPtr = nullptr;
			OutRowPitchInPixels = 0;
		}
	}

	void FRHIGPUTextureReadback::Unlock()
	{
		ensure(DestinationStagingTexture);

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		RHICmdList.UnmapStagingSurface(DestinationStagingTexture);

		CaptureStatus = ECaptureStatus::Idle;
		CapturedTime = 0;
		CapturedDuration = 0;
	}
};


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
		FIntPoint Resolution;
		{
			FScopeLogTime timecr(TEXT("Lock/Unlock"));
			void* VoidTextureData = nullptr;
			PreviousGpuReadback->LockTexture(RHICmdList, VoidTextureData, Resolution);
			TextureData = static_cast<uint8*>(VoidTextureData);
		}

		UE_LOG(LogRecorder, Verbose,
		       TEXT("SourceResolution: (Width=%d, Height=%d) RecordArea: %s"), w, h, *RecordArea.ToString())

		// 强制截取的视频帧包含的画面宽高有效，主要针对 Resize 场景做安全保证
		auto ClippedRect = RecordArea;
		ClippedRect.Max.ComponentMin(FIntPoint(Resolution.X, h));
		{
			Rst = GetOnSendFrame().ExecuteIfBound(
				TextureData, BackBuffer->GetFormat(), Resolution.X, h, ClippedRect,
				PreviousGpuReadback->CapturedTime, PreviousGpuReadback->CapturedDuration);
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

	FIntPoint Resolution(w, h);
	uint8* TextureData;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("LockTexture2D");
		uint32 TextureRowStrip;
		TextureData = (uint8*)list.LockTexture2D(BackBuffer->GetTexture2D(),
		                                         0, EResourceLockMode::RLM_ReadOnly, TextureRowStrip, false);
		int FrameWidth = TextureRowStrip / 4;
		if (RecordArea.Width() != FrameWidth && FrameWidth != 0)
		{
			Resolution.X = FrameWidth;
		}
	}

	bool Rst = false;
	// 强制截取的视频帧包含的画面宽高有效，主要针对 Resize 场景做安全保证
	auto ClippedRect = RecordArea;
	ClippedRect.Max.ComponentMin(FIntPoint(Resolution.X, h));
	{
		Rst = GetOnSendFrame().ExecuteIfBound(
			TextureData, BackBuffer->GetFormat(), Resolution.X, h, ClippedRect,
			CaptureTsInSeconds, DurationInSeconds);
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
		double Ct = TimeManager.GetNextOutputTimestamp();

		bool RecordRst;
		if (CVarRecordFrameRemapEnabled->GetBool())
		{
			RecordRst = CopyTextureToQueue_GpuReadToCpu(BackBuffer, Ct, FApp::GetDeltaTime(), CropArea);
		}
		else
		{
			RecordRst = CopyTextureToQueue_LockTextureToCpu(BackBuffer, Ct, FApp::GetDeltaTime(), CropArea);
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
