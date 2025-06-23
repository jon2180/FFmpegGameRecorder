// Copyright Epic Games, Inc. All Rights Reserved.

#include "FFmpegGameRecorder.h"

#include "Interfaces/IPluginManager.h"
#include "Runtime/Projects/Private/PluginManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FFFmpegGameRecorderModule"

void FFFmpegGameRecorderModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if PLATFORM_WINDOWS

    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    FString BaseDir = FPluginManager::Get().FindPlugin("FFmpegGameRecorder")->GetBaseDir();

    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    x264lLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/libx264-164.dll"));
    AVUtilLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/avutil-58.dll"));
    SWResampleLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/swresample-4.dll"));
    AVCodecLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/avcodec-60.dll"));
    AVFormatLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/avformat-60.dll"));
    SWScaleLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/swscale-7.dll"));
    //PostProcLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/postproc-57.dll"));
    AVFilterLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/avfilter-9.dll"));
    //AVDeviceLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/ffmpeg/lib/Win64dll/avdevice-60.dll"));
    YuvLibrary = LoadLibrary(ToCStr(BaseDir), TEXT("ThirdParty/libyuv/lib/Win64dll/libyuv.dll"));

#elif PLATFORM_MAC

    x264lLibrary = LoadLibrary(TEXT(X264_LIBRARY_PATH),TEXT("libx264.164.dylib"));
    AVUtilLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libavutil.56.dylib"));
    SWResampleLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libswresample.3.dylib"));
    AVCodecLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libavcodec.58.dylib"));
    AVFormatLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libavformat.58.dylib"));
    SWScaleLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libswscale.5.dylib"));
    // PostProcLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libpostproc.55.dylib"));
    AVFilterLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libavfilter.7.dylib"));
    // AVDeviceLibrary = LoadLibrary(TEXT(FFMPEG_LIBRARY_PATH),TEXT("libavdevice.58.dylib"));

#endif

    Initialized = true;

}

void FFFmpegGameRecorderModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    if (!Initialized)
    {
        return;
    }

    if (AVDeviceLibrary)
        FPlatformProcess::FreeDllHandle(AVDeviceLibrary);
    if (AVFilterLibrary)
        FPlatformProcess::FreeDllHandle(AVFilterLibrary);
    if (PostProcLibrary)
        FPlatformProcess::FreeDllHandle(PostProcLibrary);
    if (SWScaleLibrary)
        FPlatformProcess::FreeDllHandle(SWScaleLibrary);
    if (AVFormatLibrary)
        FPlatformProcess::FreeDllHandle(AVFormatLibrary);
    if (AVCodecLibrary)
        FPlatformProcess::FreeDllHandle(AVCodecLibrary);
    if (SWResampleLibrary)
        FPlatformProcess::FreeDllHandle(SWResampleLibrary);
    if (AVUtilLibrary)
        FPlatformProcess::FreeDllHandle(AVUtilLibrary);
    if (x264lLibrary)
        FPlatformProcess::FreeDllHandle(x264lLibrary);
    if (YuvLibrary)
        FPlatformProcess::FreeDllHandle(YuvLibrary);

    Initialized = false;
}

void *FFFmpegGameRecorderModule::LoadLibrary(const TCHAR *BasePath, const TCHAR *LibraryName)
{
    FString LibraryPath = FPaths::Combine(BasePath, LibraryName);
    UE_LOG(LogTemp, Log, TEXT("LoadLibrary: %s"), *LibraryPath);
    return FPlatformProcess::GetDllHandle(*LibraryPath);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFFmpegGameRecorderModule, FFmpegGameRecorder)
