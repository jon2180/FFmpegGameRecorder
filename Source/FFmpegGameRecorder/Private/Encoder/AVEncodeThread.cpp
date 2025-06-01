#include "Encoder/AVEncodeThread.h"

#include "Encoder/AVEncoder.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

FAVEncodeThread::FAVEncodeThread(const TSharedPtr<FAVBufferedEncoder>& InAVEncoder)
	: AVEncoder(InAVEncoder)
{
	bInitialized = true;
	bTaskRunning = true;
	Thread = FRunnableThread::Create(this, TEXT("AudioEncodeThread"));
}

bool FAVEncodeThread::Init()
{
	return true;
}

uint32 FAVEncodeThread::Run()
{
	// 居然有先 Run 再执行构造函数的例子，Thread 属性必须在构造的最后阶段创建，不然会导致出现问题，保证正确触发
	while (!bInitialized)
	{
		FPlatformProcess::Sleep(0.001);
	}

	while (bTaskRunning.load())
	{
		if (AVEncoder.IsValid())
		{
			AVEncoder->EncodeFrame_EncoderThread(true);
		}
	}
	if (AVEncoder.IsValid())
	{
		AVEncoder->Finalize_EncoderThread();
	}
	return 0;
}

void FAVEncodeThread::Stop()
{
	bTaskRunning.store(false);
	if (AVEncoder)
	{
		AVEncoder->ReleaseBufferWait(true);
	}

	FRunnable::Stop();
}

void FAVEncodeThread::Kill()
{
	Thread->WaitForCompletion();
	Thread->Kill(true);
}
