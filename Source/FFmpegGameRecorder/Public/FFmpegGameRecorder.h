// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FFMPEGGAMERECORDER_API FFFmpegGameRecorderModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void* LoadLibrary(const TCHAR* BasePath, const TCHAR* LibraryName);

    void* x264lLibrary = nullptr;
    void* AVUtilLibrary = nullptr;
    void* SWResampleLibrary = nullptr;
    void* AVCodecLibrary = nullptr;
    void* SWScaleLibrary = nullptr;
    void* AVFormatLibrary = nullptr;
    void* PostProcLibrary = nullptr;
    void* AVFilterLibrary = nullptr;
    void* AVDeviceLibrary = nullptr;
    void* YuvLibrary = nullptr;

    bool Initialized = false;
};
