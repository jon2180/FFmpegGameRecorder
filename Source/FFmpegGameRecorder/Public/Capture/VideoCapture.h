#pragma once

#include "AVRecorderBase.h"
#include "MultiGPU.h"
#include "RecorderConfig.h"

#if 0
#include "RHIFwd.h"
#endif

#include "RHIResources.h"
#include "Widgets/SWindow.h"

struct FResolveParams;
struct FResolveRect;
class FRHITexture;

namespace recorder
{
	/** Texture readback implementation. */
	class FRHIGPUTextureReadback
	{
	public:
		enum class ECaptureStatus : int8
		{
			Idle, Capturing, Captured, Encoding
		};

		FRHIGPUTextureReadback(FName RequestName, FIntPoint Resolution);
		~FRHIGPUTextureReadback();

		/** Indicates if the data is in place and ready to be read. */
		FORCEINLINE bool IsReady() const
		{
			return !Fence || (Fence->NumPendingWriteCommands.GetValue() == 0 && Fence->Poll());
		}

		/** Indicates if the data is in place and ready to be read on a subset of GPUs. */
		FORCEINLINE bool IsReady(FRHIGPUMask GPUMask) const
		{
			return !Fence || Fence->Poll(GPUMask);
		}

		void PreEnqueue(double InCapturedTime, double InCapturedDuration)
		{
			CapturedTime = InCapturedTime;
			CapturedDuration = InCapturedDuration;
		}

		void EnqueueCopyRDG(FRHICommandList& RHICmdList, FRHITexture* SourceTexture, FResolveRect Rect);
		void EnqueueCopy(FRHICommandList& RHICmdList, FRHITexture* SourceTexture, FResolveRect Rect);

		// void* Lock(uint32 NumBytes);
		void LockTexture(FRHICommandListImmediate& RHICmdList, void*& OutBufferPtr, FIntPoint& OutRowPitchInPixels);

		/**
		 * Signals that the host is finished reading from the backing buffer.
		 */
		void Unlock();

		FORCEINLINE ECaptureStatus GetCaptureStatus() const
		{
			return CaptureStatus;
		}

		FORCEINLINE const FRHIGPUMask& GetLastCopyGPUMask() const
		{
			return LastCopyGPUMask;
		}

	protected:
		FGPUFenceRHIRef Fence;
		FRHIGPUMask LastCopyGPUMask;

		/**
		 * 分辨率
		 */
		FIntPoint Resolution;

	public:
		/**
		 * 帧出现时间
		 */
		double CapturedTime;
		/**
		 * 帧展示时长
		 */
		double CapturedDuration;

	protected:
		/**
		 * 状态
		 */
		ECaptureStatus CaptureStatus;

	private:
		void EnqueueCopyInternal(FRHICommandList& RHICmdList, FRHITexture* SourceTexture, FResolveParams Params);

		FTextureRHIRef DestinationStagingTexture;
	};
}

class FScreenCaptureTimeManager
{
public:
	void Initialize(double InOutputFrameRate);
    
	// 返回是否需要处理当前输入帧
	bool ShouldProcessThisFrame(double InInputTime);
    
	// 获取下一个输出帧的时间戳
	double GetNextOutputTimestamp() const;
    
private:
	/** 时间间隔 */
	double OutputFrameInterval = 0.0;

	/** 上一帧的输出时间 */
	double LastOutputTimestamp = 0.0;
	/** 时间累加器，用于判断两帧之间的时间间隔 */
	double InputTimeAccumulator = 0.0;

	/** 录制开始的时间点（全局时间） */
	double RecordStartVideoTimeClock{0.0};
};

void FScreenCaptureTimeManager::Initialize(double InOutputFrameRate)
{
	OutputFrameInterval = InOutputFrameRate;
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
	// 时基累积算法
	InputTimeAccumulator += (InInputTime - RecordStartVideoTimeClock) - LastOutputTimestamp;

	// 累加器判定
	constexpr double TIME_SPAN_TOLERANCE = 0.10;
	constexpr double DeltaTolerance = 1 - TIME_SPAN_TOLERANCE;
	if (InputTimeAccumulator < OutputFrameInterval * DeltaTolerance)
	{
		return false;
	}
	InputTimeAccumulator -= OutputFrameInterval;

	LastOutputTimestamp += OutputFrameInterval;
	UE_LOG(LogRecorder, Display, TEXT("Time: RawTime=%lf, Current=%lf, Accumulator==%lf"),
		InInputTime, LastOutputTimestamp, InputTimeAccumulator);
	return true;
}

double FScreenCaptureTimeManager::GetNextOutputTimestamp() const
{
	return LastOutputTimestamp;
}

DECLARE_DELEGATE_SevenParams(FOnSendFrame,
                             uint8* FrameData, EPixelFormat PixelFormat, uint16 FrameWidth, uint16 FrameHeight,
                             FIntRect CaptureRect, double PresentTime, double Duration);
DECLARE_DELEGATE(FOnForceStopRecord);

/**
 * 不能再次引用 BackBuffer，否则 Resize 时会因为引用检查而崩溃
 */
class FVideoCapture final : public IAVRecorderBase
{
public:
	FVideoCapture();
	virtual ~FVideoCapture() override;

	/** IAVRecorderBase Implementations */
	virtual void Register(UWorld* World) override;
	virtual void Unregister() override;

	FOnSendFrame& GetOnSendFrame() { return OnSendFrame; }
	FOnForceStopRecord& GetOnForceStopRecord() { return OnForceStopRecord; }

	bool Initialize(const FIntPoint &InResolution, const FIntRect &InCropArea, const double InFrameRate)
	{
		if (InCropArea.Size() != InResolution)
		{
			UE_LOG(LogRecorder, Error, TEXT("VideoCapture Resolution(%s) must equal to cropped area(%s)"),
				   *InResolution.ToString(), *InCropArea.Size().ToString())
			return false;
		}
		bInitialized = true;
		Resolution = InResolution;
		CropArea = InCropArea;
		VideoTickTime = 1.0 / FMath::Max(1.0, InFrameRate);
		TimeManager.Initialize(VideoTickTime);
		return true;
	}
private:
	bool bInitialized = false;
	bool bUseFixedTimeStep = false;

	//~begin 配置
	/** 帧时长 */
	float VideoTickTime;
	/** 目标分辨率 */
	FIntPoint Resolution;
	/** 截取区域 */
	FIntRect CropArea;
	//~end 配置
private:
	/**
	 * Copy the specified texture to an internal buffer.
	 *
	 * This is a bit awkward, but it's somewhat required to account for how webrtc works (for PixelStreaming), where
	 * encoding a frame is a two step process :
	 * 1st - CopyTexture initiates a copy if the texture to the internal buffers, and returns an Id the caller can use to reference that internal buffer.
	 * 2nd - Either a Drop or Encode is required for each CopyTexture call, otherwise the respective internal buffer will stay
	 * @param BackBuffer Texture to copy
	 * @param CaptureTsInSeconds Capture timestamp
	 * @param DurationInSeconds Delta time from the previous frame
	 * @param RecordArea If {0,0}, the copy will be the same size as the passed texture. If not {0,0}, it will use that specified resolution
	 * marked as used.
	 *
	 * @note This MUST be called from the render thread
	 */
	bool CopyTextureToQueue_GpuReadToCpu(const FTexture2DRHIRef& BackBuffer, double CaptureTsInSeconds,
	                                     double DurationInSeconds,
	                                     const FIntRect& RecordArea);

	bool CopyTextureToQueue_LockTextureToCpu(const FTexture2DRHIRef& BackBuffer, double CaptureTsInSeconds,
	                                         double DurationInSeconds,
	                                         const FIntRect& RecordArea);

	void OnPreResizeWindowBackBuffer(void* BackBuffer);

	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);

	/** 注意，只在因为错误强行停止时能调用 */
	void StopRecord();
	void EndWindowReader(const bool i);
	void EndWindowReader_StandardGame(void* i);

	/** 强制停止时，外部可能需要专门处理 */
	FOnForceStopRecord OnForceStopRecord;
	/** 发送帧 */
	FOnSendFrame OnSendFrame;

	std::atomic_bool bRecording{false};
	FCriticalSection VideoCaptureCS;

	// 读取 Buffer，这里设为 1 是因为暂时没有设计适应多个 ReadBack 缓存的解析逻辑，后续可以加
	static constexpr int32 READBACK_BUFFER_COUNT = 3;
	TUniquePtr<recorder::FRHIGPUTextureReadback> GPUTextureReadback[READBACK_BUFFER_COUNT];
	int32 CurrentReadbackIndex = 0;

	// 设备相关的
	SWindow* GameWindow;

	// 视频
	FScreenCaptureTimeManager TimeManager;
};
