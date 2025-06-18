#include "Capture/RHITextureReadback.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Engine/GameViewportClient.h"

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
