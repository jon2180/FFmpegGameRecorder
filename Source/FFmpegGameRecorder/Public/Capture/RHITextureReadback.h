#pragma once

#include "MultiGPU.h"
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
