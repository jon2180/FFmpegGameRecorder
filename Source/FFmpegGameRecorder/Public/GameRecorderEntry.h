#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GameRecorderEntry.generated.h"

class UFFmpegRecorder;
class FAVEncoder;
/**
 *
 */
UCLASS()
class FFMPEGGAMERECORDER_API UGameRecorderEntry : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    UFUNCTION(BlueprintCallable)
    static UWorld* GetWorldContext(UObject* WorldContextObject);

    static FString GetSavePath();

public:
    UFUNCTION(BlueprintCallable)
    static FString StartRecord(int ScreenX, int ScreenY, int ScreenW, int ScreenH);

    UFUNCTION(BlueprintCallable)
    static void CaptureNextFrame();

    UFUNCTION(BlueprintCallable)
    static FString ScreenShot(int ScreenX, int ScreenY, int ScreenW, int ScreenH);

    UFUNCTION(BlueprintCallable)
    static void StopRecord();

    static TWeakObjectPtr<UFFmpegRecorder> CurrentDirector;
};
