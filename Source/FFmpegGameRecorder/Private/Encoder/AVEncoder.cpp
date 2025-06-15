#include "Encoder/AVEncoder.h"

#include "RHISurfaceDataConversion.h"
#include "StatsMisc.h"

struct FRHIR10G10B10A2;

FAVEncoder::FAVEncoder()
	: RecordConfig()
	  , outputs(nullptr)
	  , inputs(nullptr)
	  , filter_graph(nullptr)
	  , buffersink_ctx(nullptr)
	  , buffersrc_ctx(nullptr)
	  , video_index(0)
	  , audio_index(0)
	  , out_format_context(nullptr)
	  , video_encoder_codec_context(nullptr)
	  , audio_encoder_codec_context(nullptr)
	  , out_video_stream(nullptr)
	  , out_audio_stream(nullptr)
	  , audio_swr(nullptr)
	  , audio_frame(nullptr)
	  , video_frame(nullptr)
{
	outs[0] = nullptr;
	outs[1] = nullptr;
}

void FAVEncoder::InitializeEncoder(FRecorderConfig InRecordConfig)
{
	RecordConfig = InRecordConfig;

	// InputSampleRate = InRecordConfig.

	// avfilter_register_all();
	// av_register_all();
	// #if !PLATFORM_WINDOWS
	// return;
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	av_log_set_level(AV_LOG_WARNING);
#else
	av_log_set_level(AV_LOG_DEBUG);
#endif
	av_log_set_callback(FFmpegLogCallback);
	// #endif

	avformat_network_init();

	outs[0] = static_cast<uint8_t*>(FMemory::Realloc(outs[0], 1024 * sizeof(float)));
	outs[1] = static_cast<uint8_t*>(FMemory::Realloc(outs[1], 1024 * sizeof(float)));

	filter_descr = FString::Printf(TEXT("[in]scale=%d:%d[out]"), RecordConfig.Resolution.X, RecordConfig.Resolution.Y);

	int IsUseRTMP = RecordConfig.SaveFilePath.Find("rtmp");
	if (IsUseRTMP == 0)
	{
		if (avformat_alloc_output_context2(&out_format_context, nullptr, "flv",
		                                   TCHAR_TO_ANSI(*RecordConfig.SaveFilePath))
			< 0)
		{
			check(false);
		}
	}
	else
	{
		if (avformat_alloc_output_context2(&out_format_context, nullptr, nullptr,
		                                   TCHAR_TO_ANSI(*RecordConfig.SaveFilePath)) <
			0)
		{
			check(false);
		}
	}

	//create audio encoder
	CreateAudioSwr();
	CreateAudioEncoder("aac");

	//create video encoder
	CreateVideoEncoder(RecordConfig.bUseHardwareEncoding, TCHAR_TO_ANSI(*RecordConfig.SaveFilePath),
	                   RecordConfig.VideoBitRate);
	AllocVideoFilter();
}


void FAVEncoder::CreateAudioEncoder(const char* audioencoder_name)
{
	const AVCodec* audioencoder_codec;
	audioencoder_codec = avcodec_find_encoder_by_name(audioencoder_name);
	out_audio_stream = avformat_new_stream(out_format_context, audioencoder_codec);
	audio_index = out_audio_stream->index;
	audio_encoder_codec_context = avcodec_alloc_context3(audioencoder_codec);

	if (!out_audio_stream)
	{
		check(false);
	}
	audio_encoder_codec_context->codec_id = AV_CODEC_ID_AAC;
	constexpr int AudioBitRate = 128 * 1024;
	audio_encoder_codec_context->bit_rate = AudioBitRate;
	audio_encoder_codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
	audio_encoder_codec_context->sample_rate = RecordConfig.AudioSampleRate;
	audio_encoder_codec_context->sample_fmt = AV_SAMPLE_FMT_FLTP;
	audio_encoder_codec_context->channel_layout = AV_CH_LAYOUT_STEREO;
	audio_encoder_codec_context->channels = av_get_channel_layout_nb_channels(
		audio_encoder_codec_context->channel_layout);

	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
	{
		audio_encoder_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	audio_encoder_codec_context->codec_tag = 0;
	out_audio_stream->codecpar->codec_tag = 0;

	if (avcodec_open2(audio_encoder_codec_context, audioencoder_codec, nullptr) < 0)
	{
		check(false);
	}
	avcodec_parameters_from_context(out_audio_stream->codecpar, audio_encoder_codec_context);

	audio_frame = av_frame_alloc();
	audio_frame->nb_samples = audio_encoder_codec_context->frame_size;
	audio_frame->sample_rate = audio_encoder_codec_context->sample_rate;
	// UE_LOG(LogRecorder, Warning, TEXT("audio_frame->nb_samples %d"), audio_frame->nb_samples)
	audio_frame->format = audio_encoder_codec_context->sample_fmt;
	audio_frame->channels = audio_encoder_codec_context->channels;
	audio_frame->channel_layout = audio_encoder_codec_context->channel_layout;
}

void FAVEncoder::CreateVideoEncoder(bool is_use_NGPU, const char* out_file_name, int bit_rate)
{
	const AVCodec* encoder_codec;
	int ret;

	if (is_use_NGPU)
	{
		encoder_codec = avcodec_find_encoder_by_name("nvenc_h264");
	}
	else
	{
		encoder_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	}
	if (!encoder_codec)
	{
		check(false);
	}
	out_video_stream = avformat_new_stream(out_format_context, encoder_codec);

	video_index = out_video_stream->index;

	video_encoder_codec_context = avcodec_alloc_context3(encoder_codec);
	if (!video_encoder_codec_context)
	{
		check(false);
	}
	video_encoder_codec_context->bit_rate = bit_rate;
	video_encoder_codec_context->rc_min_rate = bit_rate;
	video_encoder_codec_context->rc_max_rate = bit_rate;
	//video_encoder_codec_context->bit_rate_tolerance = bit_rate;
	//video_encoder_codec_context->rc_buffer_size = bit_rate;
	//video_encoder_codec_context->rc_initial_buffer_occupancy = bit_rate * 3 / 4;
	video_encoder_codec_context->width = RecordConfig.Resolution.X;
	video_encoder_codec_context->height = RecordConfig.Resolution.Y;
	video_encoder_codec_context->max_b_frames = 2;
	video_encoder_codec_context->time_base.num = 1;
	video_encoder_codec_context->time_base.den = RecordConfig.FrameRate;
	video_encoder_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
	video_encoder_codec_context->me_range = 16;
	video_encoder_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
	video_encoder_codec_context->profile = FF_PROFILE_H264_BASELINE;
	video_encoder_codec_context->frame_number = 1;
	video_encoder_codec_context->qcompress = 0.8;
	video_encoder_codec_context->max_qdiff = 4;
	video_encoder_codec_context->level = 30;
	video_encoder_codec_context->gop_size = 25;
	video_encoder_codec_context->qmin = 18;
	video_encoder_codec_context->qmax = 28;
	video_encoder_codec_context->me_range = 16;
	video_encoder_codec_context->framerate = {RecordConfig.FrameRate, 1};
	// https://blog.csdn.net/wss260046582/article/details/122238453 解决 avcodec_receive_packet 在初始开始录制时会等待缓冲区填满后再读取的问题
	// av_opt_set(video_encoder_codec_context->priv_data, "tune", "zerolatency", 0);

	if (encoder_codec)
	{
		//ultrafast,superfast, veryfast, faster, fast, medium, slow, slower, veryslow,placebo.
		av_opt_set(video_encoder_codec_context->priv_data, "preset", "ultrafast", 0);
		const FString Crf = FString::Printf(TEXT("%d"), FMath::Clamp(ConstantRateFactor, 0, 51));
		av_opt_set(video_encoder_codec_context->priv_data, "crf", TCHAR_TO_UTF8(*Crf), 0);
	}

	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
	{
		video_encoder_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	int i = avcodec_open2(video_encoder_codec_context, encoder_codec, nullptr);
	if (i < 0)
	{
		check(false);
	}

	ret = avcodec_parameters_from_context(out_video_stream->codecpar, video_encoder_codec_context);
	if (ret < 0)
	{
		check(false);
	}

	video_frame = av_frame_alloc();
	if (!video_frame)
	{
		check(false);
	}

	ret = avio_open(&out_format_context->pb, out_file_name, AVIO_FLAG_WRITE);

	if (ret < 0)
	{
		UE_LOG(LogRecorder, Error, TEXT("avio_open:%d; out_file_name:%s"), ret, ANSI_TO_TCHAR(out_file_name));

		check(false);
	}
	ret = av_image_alloc(
		video_frame->data,
		video_frame->linesize,
		RecordConfig.Resolution.X,
		RecordConfig.Resolution.Y,
		video_encoder_codec_context->pix_fmt,
		32);
	if (ret < 0)
	{
		check(false);
	}

	UE_LOG(LogRecorder, Warning, TEXT("try to write header"))
	if (avformat_write_header(out_format_context, nullptr) < 0)
	{
		check(false);
	}
	UE_LOG(LogRecorder, Warning, TEXT("write header successfully"))

	// Video_Frame_Duration = out_video_stream->time_base.den / video_fps;
}

void FAVEncoder::ChangeColorFormat(AVFrame* InVideoFrame, uint8_t* FrameDataInRgb) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("ChangeColorFormat");
#if PLATFORM_MAC || PLATFORM_IOS
    libyuv::ARGBToI420(FrameDataInRgb, RecordConfig.Resolution.X * 4,
        InVideoFrame->data[0], InVideoFrame->linesize[0],
        InVideoFrame->data[1], InVideoFrame->linesize[1],
        InVideoFrame->data[2], InVideoFrame->linesize[2],
        RecordConfig.Resolution.X, RecordConfig.Resolution.Y);
#else
	libyuv::ABGRToI420(FrameDataInRgb, RecordConfig.Resolution.X * 4,
	                   InVideoFrame->data[0], InVideoFrame->linesize[0],
	                   InVideoFrame->data[1], InVideoFrame->linesize[1],
	                   InVideoFrame->data[2], InVideoFrame->linesize[2],
	                   RecordConfig.Resolution.X, RecordConfig.Resolution.Y);
#endif

	InVideoFrame->width = RecordConfig.Resolution.X;
	InVideoFrame->height = RecordConfig.Resolution.Y;
	InVideoFrame->format = AV_PIX_FMT_YUV420P;
}

void FAVEncoder::CreateAudioSwr()
{
	audio_swr = swr_alloc();
	av_opt_set_int(audio_swr, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(audio_swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(audio_swr, "in_sample_rate", RecordConfig.AudioSampleRate, 0);
	av_opt_set_int(audio_swr, "out_sample_rate", RecordConfig.AudioSampleRate, 0);
	av_opt_set_sample_fmt(audio_swr, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
	av_opt_set_sample_fmt(audio_swr, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
	swr_init(audio_swr);
}

void FAVEncoder::EncodeVideoFrame(TQueue<FTimeSeq>& VideoTimeSequence, FEncodeData* rgb)
{
	ChangeColorFormat(video_frame, rgb->GetRawData());

	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("Encode_Video_Frame");
	// use ffmpeg to encode frame
	AVPacket* video_pkt = av_packet_alloc();
	AVFrame* filt_frame = av_frame_alloc();

	if (av_buffersrc_add_frame_flags(buffersrc_ctx, video_frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
	{
		check(false);
	}
	FTimeSeq VideoTime;
	while (true)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("ffmpeg_encode_frame");
		int ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			break;
		}
		if (ret < 0)
		{
			break;
		}
		if (ret >= 0)
		{
			{
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("avcodec_send_frame");
				avcodec_send_frame(video_encoder_codec_context, filt_frame);
			}
			while (ret >= 0)
			{
				{
					TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("avcodec_receive_packet");
					ret = avcodec_receive_packet(video_encoder_codec_context, video_pkt);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					{
						av_packet_unref(video_pkt);
						break;
					}
					if (ret < 0)
					{
						av_packet_unref(video_pkt);
						break;
					}
				}
				// TODO 从队列中取
				if (VideoTimeSequence.Dequeue(VideoTime))
				{
					video_pkt->stream_index = video_index;
					video_pkt->pts = video_pkt->dts = floor(VideoTime.Current * out_video_stream->time_base.den);
					video_pkt->duration = VideoTime.Duration * out_video_stream->time_base.den;

					CurrentEncodeVideoTime = VideoTime.Current + VideoTime.Duration;

					UE_LOG(LogRecorder, Log,
					       TEXT("EncodeVideoFrame: VideoTime=%lf, Duration=%lf, pts=%lld, duration=%lld, den=%d"),
					       VideoTime.Current, VideoTime.Duration, video_pkt->pts, video_pkt->duration,
					       out_video_stream->time_base.den)

					{
						TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("av_write_frame");
						av_write_frame(out_format_context, video_pkt);
					}
				}
			}
			av_packet_unref(video_pkt);
		}
	}
	av_frame_unref(filt_frame);

	{
		av_frame_free(&filt_frame);
		av_packet_free(&video_pkt);
	}
}

void FAVEncoder::EncodeAudioFrame(TQueue<FTimeSeq>& AudioTimeSequence, FEncodeData* rgb)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("Encode_Audio_Frame");

	const uint8_t* data = rgb->GetRawData();
	AVPacket* audio_pkt = av_packet_alloc();

	int count = swr_convert(audio_swr, outs, audio_frame->nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLTP),
	                        &data, audio_encoder_codec_context->frame_size);

	audio_frame->data[0] = outs[0];
	audio_frame->data[1] = outs[1];
	audio_frame->nb_samples = count;
	SetAudioVolume(audio_frame);

	int ret = avcodec_send_frame(audio_encoder_codec_context, audio_frame);
	if (ret == AVERROR_EOF)
	{
		ret = 0;
	}
	else if (ret == AVERROR(EAGAIN))
	{
	}
	else if (ret < 0)
	{
		return;
	}

	// av_packet_move_ref()
	while (ret >= 0)
	{
		ret = avcodec_receive_packet(audio_encoder_codec_context, audio_pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			av_packet_unref(audio_pkt);
			break;
		}
		if (ret < 0)
		{
			av_packet_unref(audio_pkt);
			break;
		}

		audio_pkt->stream_index = audio_index;
		FTimeSeq AudioTimeSeq;
		if (AudioTimeSequence.Dequeue(AudioTimeSeq))
		{
			CurrentEncodeAudioTime = AudioTimeSeq.Current + AudioTimeSeq.Duration;
			audio_pkt->pts = audio_pkt->dts = AudioTimeSeq.Current * out_audio_stream->time_base.den;
			audio_pkt->duration = AudioTimeSeq.Duration * out_audio_stream->time_base.den;

			// audio_pkt->pts = audio_pkt->dts = av_rescale_q(
			//     (CurrentAuidoTime + audio_delay) / av_q2d({1, DefaultOutputSampleRate}),
			//     {1, DefaultOutputSampleRate},
			//     out_audio_stream->time_base);
			//
			// audio_pkt->duration = av_rescale_q(
			//     audio_pkt->duration,
			//     {1, DefaultOutputSampleRate},
			//     out_audio_stream->time_base);

			UE_LOG(LogRecorder, Log,
			       TEXT("EncodeAudioFrame: AudioTime=%lf, Duration=%lf, pts=%lld, duration=%lld, den=%d"),
			       AudioTimeSeq.Current, AudioTimeSeq.Duration, audio_pkt->pts, audio_pkt->duration,
			       out_audio_stream->time_base.den)
		}

		av_write_frame(out_format_context, audio_pkt);
	}
	av_packet_unref(audio_pkt);

	{
		// av_frame_free(&audio_frame);
		av_packet_free(&audio_pkt);
	}
}

void FAVEncoder::EndAudioEncoding(TQueue<FTimeSeq>& AudioTimeSequence)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("EndAudioEncoding");
	AVPacket* audio_pkt = av_packet_alloc();
	// av_init_packet(VideoPacket);
	int ret = avcodec_send_frame(audio_encoder_codec_context, nullptr);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	{
		av_packet_unref(audio_pkt);
		return;
	}
	if (ret < 0)
	{
		av_packet_unref(audio_pkt);
		return;
	}

	FTimeSeq Time;
	while (true)
	{
		int Ret = avcodec_receive_packet(audio_encoder_codec_context, audio_pkt);
		if (Ret == AVERROR(EAGAIN) || Ret == AVERROR_EOF)
		{
			av_packet_unref(audio_pkt);
			break;
		}
		if (Ret < 0)
		{
			av_packet_unref(audio_pkt);
			break;
		}
		audio_pkt->stream_index = audio_index;

		if (AudioTimeSequence.Dequeue(Time))
		{
			// TODO: 帧同步
			audio_pkt->pts = audio_pkt->dts = floor(Time.Current * out_audio_stream->time_base.den);
			audio_pkt->duration = Time.Duration * out_audio_stream->time_base.den;

			CurrentEncodeAudioTime = Time.Current + Time.Duration;

			UE_LOG(LogRecorder, Log,
			       TEXT("EndAudioEncoding: AudioTime=%lf, Duration=%lf, pts=%lld, duration=%lld, den=%d"),
			       Time.Current, Time.Duration, audio_pkt->pts, audio_pkt->duration, out_audio_stream->time_base.den)

			Ret = av_write_frame(out_format_context, audio_pkt);
		}

		if (Ret < 0)
		{
			UE_LOG(LogRecorder, Warning, TEXT("EncodeAudioFrame: audio_pkt->pts %lld duration %lld rst %lf"),
			       audio_pkt->pts, audio_pkt->duration,
			       static_cast<double>(audio_pkt->pts + audio_pkt->duration) / out_audio_stream->time_base.den);
		}
	}

	if (!AudioTimeSequence.IsEmpty())
	{
		FTimeSeq Time1;
		int32 LeftNum = 0;
		while (!AudioTimeSequence.IsEmpty())
		{
			LeftNum += 1;
			AudioTimeSequence.Dequeue(Time1);
			// UE_LOG(LogRecorder, Warning, TEXT("EncodeAudioFrame: time start: %lf duration: %lf"), Time1.Current,
			//     Time1.Duration);
		}
		UE_LOG(LogRecorder, Warning, TEXT("%d remain audio frames in queue discarded"), LeftNum)
	}

	av_packet_unref(audio_pkt);
	// av_frame_unref(audio_frame);

	FMemory::Free(outs[0]);
	FMemory::Free(outs[1]);
	// FMemory::Free(buff_bgr);
}

void FAVEncoder::EndVideoEncoding(TQueue<FTimeSeq>& VideoTimeSequence)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("EndVideoEncoding");
	AVPacket* VideoPacket = av_packet_alloc();

	avcodec_send_frame(video_encoder_codec_context, nullptr);

	FTimeSeq Time{};
	while (true)
	{
		int Ret = avcodec_receive_packet(video_encoder_codec_context, VideoPacket);
		if (Ret == AVERROR(EAGAIN) || Ret == AVERROR_EOF)
		{
			av_packet_unref(VideoPacket);
			break;
		}
		if (Ret < 0)
		{
			av_packet_unref(VideoPacket);
			break;
		}
		VideoPacket->stream_index = video_index;

		if (VideoTimeSequence.Dequeue(Time))
		{
			VideoPacket->pts = VideoPacket->dts = floor(Time.Current * out_video_stream->time_base.den);
			VideoPacket->duration = Time.Duration * out_video_stream->time_base.den;

			CurrentEncodeVideoTime = Time.Current + Time.Duration;

			UE_LOG(LogRecorder, Log,
			       TEXT("EndVideoEncoding: VideoTime=%lf, Duration=%lf, pts=%lld, duration=%lld, den=%d"),
			       Time.Current, Time.Duration, VideoPacket->pts, VideoPacket->duration,
			       out_video_stream->time_base.den)
			av_write_frame(out_format_context, VideoPacket);
		}
	}
	av_packet_unref(VideoPacket);
}

void FAVEncoder::SetAudioVolume(AVFrame* frame)
{
	float* ch_left = (float*)frame->data[0];
	float* ch_right = (float*)frame->data[1];
	for (int i = 0; i < frame->nb_samples; i++)
	{
		ch_left[i] *= RecordConfig.SoundVolume;
		ch_right[i] *= RecordConfig.SoundVolume;
	}
}

void FAVEncoder::AllocVideoFilter()
{
	outputs = avfilter_inout_alloc();
	inputs = avfilter_inout_alloc();
	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	enum AVPixelFormat pix_fmts[] = {video_encoder_codec_context->pix_fmt, AV_PIX_FMT_NONE};
	AVRational time_base = {1, 1000000};
	int ret = 0;

	filter_graph = avfilter_graph_alloc();
	if (!outputs || !inputs || !filter_graph)
	{
		check(false);
	}

	char args[100];
	snprintf(args, sizeof(args),
	         "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
	         video_encoder_codec_context->width, video_encoder_codec_context->height,
	         video_encoder_codec_context->pix_fmt,
	         time_base.num, time_base.den,
	         video_encoder_codec_context->sample_aspect_ratio.num,
	         video_encoder_codec_context->sample_aspect_ratio.den);

	ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
	                                   args, nullptr, filter_graph);
	if (ret < 0)
	{
		check(false);
	}
	ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
	                                   nullptr, nullptr, filter_graph);
	if (ret < 0)
	{
		check(false);
	}

	ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
	                          AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0)
	{
		check(false);
	}

	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = nullptr;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = nullptr;

	if ((ret = avfilter_graph_parse_ptr(filter_graph, TCHAR_TO_ANSI(*filter_descr),
	                                    &inputs, &outputs, nullptr)) < 0)
	{
		check(false);
	}
	if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0)
	{
		check(false);
	}
}

void FAVEncoder::EncodeFinish()
{
	if (out_format_context)
	{
		av_write_trailer(out_format_context);
		avio_close(out_format_context->pb);
		avformat_free_context(out_format_context);
		out_format_context = nullptr;
	}

	if (video_encoder_codec_context)
	{
		avcodec_free_context(&video_encoder_codec_context);
		avcodec_close(video_encoder_codec_context);
		av_free(video_encoder_codec_context);
		video_encoder_codec_context = nullptr;
	}

	if (buffersink_ctx)
	{
		avfilter_free(buffersink_ctx);
		buffersink_ctx = nullptr;
	}
	if (buffersrc_ctx)
	{
		avfilter_free(buffersrc_ctx);
		buffersrc_ctx = nullptr;
	}

	if (audio_encoder_codec_context)
	{
		avcodec_free_context(&audio_encoder_codec_context);
		avcodec_close(audio_encoder_codec_context);
		av_free(audio_encoder_codec_context);
		audio_encoder_codec_context = nullptr;
	}
	if (audio_swr)
	{
		swr_close(audio_swr);
		swr_free(&audio_swr);
		audio_swr = nullptr;
	}

	avfilter_graph_free(&filter_graph);
	filter_graph = nullptr;
	avfilter_inout_free(&inputs);
	inputs = nullptr;
	avfilter_inout_free(&outputs);
	outputs = nullptr;

	av_frame_free(&video_frame);
	video_frame = nullptr;

	av_frame_free(&audio_frame);
	audio_frame = nullptr;

	// if (out_video_stream)
	// {
	// 	// if (stream != NULL) {
	// 	// 	avcodec_free_context(&stream->codec);
	// 	// 	avformat_close_input(&stream->inFormatContext);
	// 	// 	avformat_free_context(stream->outFormatContext);
	// 	// av_frame_free(&stream->frame);
	// 	// av_packet_free(&out_video_stream->packet);
	// 	free(out_video_stream);
	// 	out_video_stream = nullptr;
	// }
	//
	// if (out_audio_stream)
	// {
	// 	free(out_audio_stream);
	// 	out_audio_stream = nullptr;
	// }
}

FAVBufferedEncoder::FAVBufferedEncoder(): ThreadEvent(nullptr)
{
	ThreadEvent = FGenericPlatformProcess::GetSynchEventFromPool();
}

FAVBufferedEncoder::~FAVBufferedEncoder()
{
	if (ThreadEvent)
	{
		FGenericPlatformProcess::ReturnSynchEventToPool(ThreadEvent);
		ThreadEvent = nullptr;
	}

	FEncodeData* NewData;

	{
		FScopeLock Lock(&VideoMutex);
		while (!VideoBufferPool.IsEmpty())
		{
			if (VideoBufferPool.Dequeue(NewData))
			{
				delete NewData;
				NewData = nullptr;
			}
		}

		while (!VideoBuffer.IsEmpty())
		{
			if (VideoBuffer.Dequeue(NewData))
			{
				delete NewData;
				NewData = nullptr;
			}
		}

		VideoTimeSequence.Empty();
	}

	{
		FScopeLock Lock(&AudioMutex);
		while (!AudioBufferPool.IsEmpty())
		{
			if (AudioBufferPool.Dequeue(NewData))
			{
				delete NewData;
				NewData = nullptr;
			}
		}

		while (!AudioBuffer.IsEmpty())
		{
			if (AudioBuffer.Dequeue(NewData))
			{
				delete NewData;
				NewData = nullptr;
			}
		}
		AudioTimeSequence.Empty();
	}
}

void FAVBufferedEncoder::Initialize(/*TSharedPtr<FAVEncoder>& InAVEncoder*/)
{
	// Encoder = InAVEncoder;
	Encoder = MakeShared<FAVEncoder>();
}

bool FAVBufferedEncoder::WaitBufferInsert(bool bForce)
{
	if (bForce || (VideoBuffer.IsEmpty() && !Encoder->ShouldContinueAudioEncoding()))
	{
		if (ThreadEvent)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("EncodeThreadWait");
			ThreadEvent->Wait();
		}
	}
	return true;
}

bool FAVBufferedEncoder::ReleaseBufferWait(bool bForce)
{
	if (bForce || !(VideoBuffer.IsEmpty() && !Encoder->ShouldContinueAudioEncoding()))
	{
		if (ThreadEvent)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("EncodeThreadRelease");
			ThreadEvent->Trigger();
		}
	}
	return true;
}

void FAVBufferedEncoder::EncodeFrame_EncoderThread(bool bWaitIfBufferEmpty)
{
	EncodeOneVideoFrame_EncoderThread();
	EncodeAudioFrames_EncoderThread();
	if (bWaitIfBufferEmpty)
	{
		WaitBufferInsert();
	}
}

void FAVBufferedEncoder::EncodeOneVideoFrame_EncoderThread()
{
	check(!IsInRenderingThread())

	UE_LOG(LogRecorder, Verbose, TEXT("EncodeOneVideoFrame_EncoderThread"))
	if (!VideoBuffer.IsEmpty())
	{
		FEncodeData* EncodeData;
		if (VideoBuffer.Dequeue(EncodeData))
		{
			Encoder->EncodeVideoFrame(VideoTimeSequence, EncodeData);
			VideoBufferPool.Enqueue(EncodeData);
		}
	}
}

void FAVBufferedEncoder::EncodeAudioFrames_EncoderThread()
{
	check(!IsInAudioThread())

	UE_LOG(LogRecorder, Verbose, TEXT("EncodeAudioFrames_EncoderThread"))

	// 音频应该直接编码到最新的视频的一帧的时间点
	while (!AudioBuffer.IsEmpty() && Encoder->ShouldContinueAudioEncoding())
	{
		FEncodeData* EncodeData;
		if (AudioBuffer.Dequeue(EncodeData))
		{
			Encoder->EncodeAudioFrame(AudioTimeSequence, EncodeData);
			AudioBufferPool.Enqueue(EncodeData);
		}
	}
}

void FAVBufferedEncoder::Finalize_EncoderThread()
{
	FinalizeVideoFrames_EncoderThread();
	FinalizeAudioFrames_EncoderThread();
	Encoder->EncodeFinish();
}


void FAVBufferedEncoder::FinalizeVideoFrames_EncoderThread()
{
	UE_LOG(LogRecorder, Verbose, TEXT("FinalizeVideoFrames_EncoderThread"))

	// 清理当前 Buffer 中的视频
	while (!VideoBuffer.IsEmpty())
	{
		EncodeOneVideoFrame_EncoderThread();
	}

	// 清理 buffer
	Encoder->EndVideoEncoding(VideoTimeSequence);
}

void FAVBufferedEncoder::FinalizeAudioFrames_EncoderThread()
{
	UE_LOG(LogRecorder, Verbose, TEXT("FinalizeAudioFrames_EncoderThread"))

	EncodeAudioFrames_EncoderThread();
	Encoder->EndAudioEncoding(AudioTimeSequence);
}

void FAVBufferedEncoder::EnqueueVideoFrame_RenderThread(uint8* FrameData, EPixelFormat PixelFormat,
                                                        uint16 FrameWidth, uint16 FrameHeight, FIntRect CaptureRect,
                                                        double PresentTime, double Duration)
{
	FScopeLogTime timerin(TEXT("InsertVideoToEnqueue"));
	UE_LOG(LogRecorder, Display, TEXT("CaptureVideoFrameReadyToSend"));
	FEncodeData* NewData = nullptr;
	if (VideoBufferPool.IsEmpty())
	{
		NewData = new FEncodeData();
		// NewData->Initialize(DataSize);
	}
	else
	{
		VideoBufferPool.Dequeue(NewData);
	}

	if (NewData)
	{
		FScopeLock Lock(&NewData->ModifyCS);
		NewData->StartSec = PresentTime;
		NewData->Duration = Duration;

		NewData->Data.SetNumUninitialized(FrameWidth * FrameHeight);
#if PLATFORM_WINDOWS
		if (PixelFormat == EPixelFormat::PF_A2B10G10R10)
		{
			auto Src = FrameData;
			uint32 Stride = FrameWidth * 4;
			// uint32 Stride = (CaptureRect.Max.X - CaptureRect.Min.X) * 4;
			uint8* DestPtr = NewData->GetRawData();
			uint8* SrcPtr = nullptr;

			// #if PLATFORM_WINDOWS
			// FScopeLogTime t(TEXT("CopyPixels"));
			// #endif
			// FMemory::Memcpy(Data, Src, DataLength);
			FRHIR10G10B10A2* Pixel = nullptr;
			if (false/* bForceFullScreen */)
			{
				CaptureRect.Min.X = 0;
				CaptureRect.Min.Y = 0;
				CaptureRect.Max.X = FrameWidth;
				CaptureRect.Max.Y = FrameHeight;
			}
			for (int32 Y = CaptureRect.Min.Y; Y < CaptureRect.Max.Y; ++Y)
			{
				SrcPtr = Src + Y * Stride + CaptureRect.Min.X * 4;
				for (int32 X = CaptureRect.Min.X; X < CaptureRect.Max.X; ++X)
				{
					Pixel = reinterpret_cast<FRHIR10G10B10A2*>(SrcPtr);
					*(DestPtr + 0) = Requantize10to8(Pixel->R);
					*(DestPtr + 1) = Requantize10to8(Pixel->G);
					*(DestPtr + 2) = Requantize10to8(Pixel->B);
					*(DestPtr + 3) = Requantize10to8(Pixel->A);
					DestPtr += 4;
					SrcPtr += 4;
				}
			}
		}
#endif

#if PLATFORM_ANDROID
    {
        TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("FlipViedo");
        uint32 Stride = FrameWidth * 4;
        // 假定成立 如果不成立需要特殊处理
        // checkf(LolStride == Stride, TEXT("Encode_Video_Frame Stride != LolStride"));

        int32 CopyBytes = 4 * (CaptureRect.Max.X - CaptureRect.Min.X);
        uint32 DestY = 0;
        for (int32 Y = CaptureRect.Min.Y; Y < CaptureRect.Max.Y; ++Y, ++DestY)
        {
            uint8_t* DestPtr = Data + DestY * CopyBytes;
            uint8_t* SrcPtr = Src + (FrameHeight - Y - 1) * Stride + 4 * CaptureRect.Min.X;
            FMemory::Memcpy(DestPtr, SrcPtr, CopyBytes);
        }
    }
#endif

#if PLATFORM_MAC || PLATFORM_IOS
    {
    	// 实际帧的宽度
        const uint32 FrameStride = FrameWidth * sizeof(FColor);
    	// 每一行的左侧的偏移
        const uint32 FrameOffsetInBytes = CaptureRect.Min.X * sizeof(float);
    	// 目标视频帧的宽度
        const uint32 DestStride = (CaptureRect.Max.X - CaptureRect.Min.X) * sizeof(float);
        
        // 按行copy
        uint8* DestPtr = Data;
        // 始终指向像素数据每一行的开头，已指向 Min.Y 所在行
        uint8* SrcPtr = Src + CaptureRect.Min.Y * FrameStride;
        for (int32 y = CaptureRect.Min.Y; y < CaptureRect.Max.Y; ++y)
        {
            FMemory::Memcpy(DestPtr, SrcPtr + FrameOffsetInBytes, DestStride);
            SrcPtr += FrameStride;
            DestPtr += DestStride;
        }
    }
#endif
	}

	VideoBuffer.Enqueue(NewData);
	VideoTimeSequence.Enqueue({PresentTime, Duration});
	ReleaseBufferWait();
}

void FAVBufferedEncoder::EnqueueAudioFrame_AudioThread(float* AudioData, int NumSamples, int32 NumChannels,
                                                       int32 SampleRate, double AudioClock, double PresentTime,
                                                       double Duration)
{
	FScopeLogTime timerin(TEXT("InsertAudioToEnqueue"));
	UE_LOG(LogRecorder, Display, TEXT("CaptureAudioFrameReadyToSend"));

	FEncodeData* NewData = nullptr;
	if (AudioBufferPool.IsEmpty())
	{
		NewData = new FEncodeData();
		// NewData->Initialize(DataSize);
	}
	else
	{
		AudioBufferPool.Dequeue(NewData);
	}

	if (NewData)
	{
		FScopeLock Lock(&NewData->ModifyCS);
		NewData->Data.SetNumUninitialized(NumSamples * NumChannels);
		FMemory::StreamingMemcpy(NewData->Data.GetData(), AudioData,
			NumSamples * NumChannels * sizeof(TArray<float>::ElementType));
		NewData->StartSec = PresentTime;
		NewData->Duration = Duration;
	}

	AudioBuffer.Enqueue(NewData);
	AudioTimeSequence.Enqueue({PresentTime, Duration});
	ReleaseBufferWait();
}
