#pragma once

THIRD_PARTY_INCLUDES_START

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"
#include "libswscale/swscale.h"
#include "libavutil/file.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/error.h"
#include "libswresample/swresample.h"
#include "libyuv/convert.h"
}

THIRD_PARTY_INCLUDES_END

void FFmpegLogCallback(void*, int Level, const char* Format, va_list ArgList);

FORCEINLINE uint32 FormatSize_X(uint32 x)
{
    while ((x % 2) != 0)
    {
        x--;
    }
    return x;
}


static uint8 Requantize10to8(int Value10);