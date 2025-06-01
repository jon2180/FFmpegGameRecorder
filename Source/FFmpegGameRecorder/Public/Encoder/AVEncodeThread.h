#pragma once

#include <atomic>

#include "HAL/Runnable.h"

class FAVBufferedEncoder;

/**
 * 音视频编码器
 */
class FFMPEGGAMERECORDER_API FAVEncodeThread : public FRunnable
{
public:
	FAVEncodeThread(const TSharedPtr<FAVBufferedEncoder>& InAVEncoder);

	virtual ~FAVEncodeThread() override = default;

	virtual bool Init() override;

	virtual uint32 Run() override;

	virtual void Stop() override;

	void Kill();

private:
	FRunnableThread* Thread;

	TSharedPtr<FAVBufferedEncoder> AVEncoder;
	std::atomic_bool bInitialized;
	std::atomic_bool bTaskRunning;
};
