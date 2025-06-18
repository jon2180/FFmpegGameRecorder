#pragma once

#include "AVRecorderBase.h"
#include "PixelFormat.h"
#include "RHITextureReadback.h"

/**
 * 控制时间捕获的输出帧率，输出的帧率范围为 Min(游戏实际帧率，预设帧率)，
 * 比如游戏 FPS 为 30，预期 60，输出的帧率应该为 30
 */
class FScreenCaptureTimeManager
{
public:
	void Initialize(double InOutputFrameRate);
    
	// 返回是否需要处理当前输入帧
	bool ShouldProcessThisFrame(double InInputTime);

	// 获取下一个输出帧的时间戳
	FORCEINLINE_DEBUGGABLE double GetNextOutputTimestamp() const { return LastOutputTimestamp; }

	FORCEINLINE_DEBUGGABLE double GetNextOutputDuration() const { return OutputFrameInterval; }
    
private:
	/** 时间间隔 */
	double OutputFrameInterval = 0.0;

	/** 上一帧的输出时间 */
	double LastOutputTimestamp = 0.0;
	/** 时间累加器，用于判断两帧之间的时间间隔 */
	double InputTimeAccumulator = 0.0;

	/** 录制开始的时间点（全局时间） */
	double RecordStartVideoTimeClock{0.0};

	/** 预期的输出视频的帧时长 */
	double ExpectedOutputFrameInterval{0.0};
};

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
