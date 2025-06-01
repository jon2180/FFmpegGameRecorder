#pragma once

#include "CoreMinimal.h"

#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "PixelFormat.h"
#include "FFmpegExt/FFmpegExtension.h"
#include "Capture/RecorderConfig.h"

class FEncoderThread;
class FEncodeData;

struct FTimeSeq
{
	/** 帧位置 */
	double Current;
	/** 帧时长 */
	double Duration;
};

class FAVEncoder
{
public:
	FAVEncoder();

	void InitializeEncoder(FRecorderConfig InRecordConfig);
	void CreateAudioEncoder(const char* audioencoder_name);
	void CreateVideoEncoder(bool is_use_NGPU, const char* out_file_name, int bit_rate);
	void ChangeColorFormat(AVFrame* InVideoFrame, uint8_t* FrameDataInRgb) const;

	void CreateAudioSwr();

	void EncodeVideoFrame(TQueue<FTimeSeq>& VideoTimeSequence, FEncodeData* rgb);
	void EncodeAudioFrame(TQueue<FTimeSeq>& AudioTimeSequence, FEncodeData* rgb);

	void EndAudioEncoding(TQueue<FTimeSeq>& AudioTimeSequence);
	void EndVideoEncoding(TQueue<FTimeSeq>& VideoTimeSequence);
	// private:
	void SetAudioVolume(AVFrame* frame);

	void AllocVideoFilter();

	void EncodeFinish();

	FORCEINLINE_DEBUGGABLE int GetAudioFrameSize() const
	{
		return audio_encoder_codec_context->frame_size;
	}

	/** 音频时间轴向视频时间轴靠近，所以音频不能过长，这里先固定一个 0.1s 的差值，意味着音频最多比视频长 0.1s */
	FORCEINLINE_DEBUGGABLE bool ShouldContinueAudioEncoding() const
	{
		if (CurrentEncodeVideoTime == 0 && CurrentEncodeAudioTime == 0) { return false; }
		return CurrentEncodeVideoTime + 0.100 >= CurrentEncodeAudioTime;
	}

	FRecorderConfig RecordConfig;

private:
	AVFilterInOut* outputs;
	AVFilterInOut* inputs;
	AVFilterGraph* filter_graph;
	AVFilterContext* buffersink_ctx;
	AVFilterContext* buffersrc_ctx;

	FString filter_descr;

	int32_t video_index;
	int32_t audio_index;

	AVFormatContext* out_format_context;
	AVCodecContext* video_encoder_codec_context;
	AVCodecContext* audio_encoder_codec_context;

	AVStream* out_video_stream;
	AVStream* out_audio_stream;

	SwrContext* audio_swr;
	uint8_t* outs[2];


	AVFrame* audio_frame;
	AVFrame* video_frame;

	// int32 CurrentAudioSendBufferIndex;
	// uint32 FormatSize_X(uint32 x);
	// [[maybe_unused]] TArray<FColor> Colors;
	// static constexpr int32 FRAME_COUNT = 2;
	// FTextureStageFrame Frame[FRAME_COUNT];
	// int32 CurrentFrameIndex;
	/** 设备音频采样率 */
	// int32 InputSampleRate;
	// uint32 InsertedFrameCount = 0;
	// uint32 DecodedFrameCount = 0;

	/**
	 * 当前编码好的音频结束时间， 在 FixTimeStep 开启时，音视频时间轴没有对齐，所以需要裁剪掉后续的音频，不加入编码队列
	 */
	double CurrentEncodeAudioTime = 0;
	/**
	 * 当前编码好的视频结束时间，优先保证视频完全编码
	 */
	double CurrentEncodeVideoTime = 0;
};

/**
 * 带 Buffer 的编码器，支持多线程同步
 * @note 之所以不内嵌在 EncoderThread 中或者 Encoder 中，是因为独立出来可以支持多个编码线程，使用同一份缓存，且使 Encoder 保持专一
 */
class FAVBufferedEncoder
{
public:
	FAVBufferedEncoder();
	~FAVBufferedEncoder();
	void Initialize(/*TSharedPtr<FAVEncoder>& InAVEncoder*/);

	FORCEINLINE_DEBUGGABLE const TSharedPtr<FAVEncoder>& GetEncoder() const { return Encoder; }
	FORCEINLINE_DEBUGGABLE TSharedPtr<FAVEncoder>& GetEncoder() { return Encoder; }

	FORCEINLINE_DEBUGGABLE bool IsVideoBufferEmpty() const { return VideoBuffer.IsEmpty(); }
	FORCEINLINE_DEBUGGABLE bool IsAudioBufferEmpty() const { return AudioBuffer.IsEmpty(); }

	bool WaitBufferInsert(bool bForce = false);
	/** 请注意，需要在编码线程结束的时候调用释放信号量 */
	bool ReleaseBufferWait(bool bForce = false);

	/** 以视频轴为基准，音频向视频对齐 */
	void EncodeFrame_EncoderThread(bool bWaitIfBufferEmpty);

private:
	/** 把最新的一帧送到编码器 */
	void EncodeOneVideoFrame_EncoderThread();
	/** 把最新的可以编码的帧送到编码器，依赖视频编码进度 */
	void EncodeAudioFrames_EncoderThread();

public:
	void Finalize_EncoderThread();

private:
	void FinalizeVideoFrames_EncoderThread();
	void FinalizeAudioFrames_EncoderThread();

public:
	void EnqueueVideoFrame_RenderThread(uint8* FrameData, EPixelFormat PixelFormat, uint16 FrameWidth,
	                                    uint16 FrameHeight,
	                                    FIntRect CaptureRect, double PresentTime, double Duration);
	void EnqueueAudioFrame_AudioThread(float* AudioData, int NumSamples, int32 NumChannels,
	                                   int32 SampleRate, double AudioClock, double PresentTime, double Duration);

private:
	TSharedPtr<FAVEncoder> Encoder;
	FEvent* ThreadEvent;

	TQueue<FEncodeData*> VideoBufferPool;
	TQueue<FEncodeData*> VideoBuffer;
	TQueue<FTimeSeq> VideoTimeSequence;
	FCriticalSection VideoMutex;

	TQueue<FEncodeData*> AudioBufferPool;
	TQueue<FEncodeData*> AudioBuffer;
	TQueue<FTimeSeq> AudioTimeSequence;
	FCriticalSection AudioMutex;

	// TODO
	// FDelegateHandleLocal EndPIEDelegateHandle;
};
