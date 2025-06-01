#include "FFmpegExt/FFmpegExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFmpeg, Log, Log);

constexpr int MAX_BUFFER_COUNT = 512;

void FFmpegLogCallback(void*, int Level, const char* Format, va_list ArgList)
{
    char LogBuffer[MAX_BUFFER_COUNT]{0};
    const int Count = vsnprintf(LogBuffer, MAX_BUFFER_COUNT, Format, ArgList);
    if (Count <= 0 || Count >= MAX_BUFFER_COUNT)
    {
        return;
    }
    LogBuffer[Count] = '\0';
    auto* LogStr = ANSI_TO_TCHAR(LogBuffer);
    switch (Level)
    {
        case AV_LOG_TRACE:
            UE_LOG(LogFFmpeg, VeryVerbose, TEXT("%s"), LogStr);
            break;
        case AV_LOG_DEBUG:
            UE_LOG(LogFFmpeg, Verbose, TEXT("%s"), LogStr);
            break;
        case AV_LOG_VERBOSE:
            UE_LOG(LogFFmpeg, Log, TEXT("%s"), LogStr);
            break;
        case AV_LOG_INFO:
            UE_LOG(LogFFmpeg, Display, TEXT("%s"), LogStr);
            break;
        case AV_LOG_WARNING:
            UE_LOG(LogFFmpeg, Warning, TEXT("%s"), LogStr);
            break;
        case AV_LOG_ERROR:
            UE_LOG(LogFFmpeg, Error, TEXT("%s"), LogStr);
            break;
        case AV_LOG_FATAL:
            UE_LOG(LogFFmpeg, Fatal, TEXT("%s"), LogStr);
            break;
        default:
            UE_LOG(LogFFmpeg, Log, TEXT("%s"), LogStr);
            break;
    }
}

uint8 Requantize10to8(int Value10)
{
    check(Value10 >= 0 && Value10 <= 1023);

    // Dequantize from 10 bit (Value10/1023.f)
    // requantize to 8 bit with rounding (GPU convention UNorm)
    //  this is the computation we want :
    // (int)( (Value10/1023.f)*255.f + 0.5f );
    // this gives the exactly the same results :
    int Temp = Value10 * 255 + (1 << 9);
    int Value8 = (Temp + (Temp >> 10)) >> 10;
    return (uint8) Value8;
}