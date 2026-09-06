#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZDBridgeLibrary.generated.h"

class UPaperSprite;
class UPaperFlipbook;
class UPaperZDAnimSequence;

UCLASS()
class ZDBRIDGE_API UZDBridgeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Paper2D", CallInEditor)
    static UPaperSprite* CreatePaperSpriteFromTexture(UTexture2D* Texture, const FString& PackagePath, const FString& AssetName, FString& Error);

    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Paper2D", CallInEditor)
    static UPaperFlipbook* CreatePaperFlipbookFromSprites(const TArray<UPaperSprite*>& Sprites, const TArray<int32>& FrameRuns, float FramesPerSecond, const FString& PackagePath, const FString& AssetName, FString& Error);

    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static UPaperZDAnimSequence* CreatePaperZDSequence(UPaperFlipbook* Flipbook, UObject* AnimationSource, const FString& PackagePath, const FString& AssetName, FString& Error);

    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static bool AddOrUpdateSupportedAnimation(UObject* AnimationSource, FName AnimationName, UPaperZDAnimSequence* Sequence, bool& Created, FString& Error);

    // Python-friendly wrapper: returns "created", "updated", or "existing";
    // returns an "error:" string instead of using reflected out parameters.
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static FString EnsureSequenceAnimationSource(UObject* AnimationSource, UPaperZDAnimSequence* Sequence);

    // Python-friendly diagnostic snapshot of all loaded PaperZD sequences bound to an AnimSource.
    // Returns a JSON string so the toolbox can compare stable asset facts without parsing editor UI state.
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static FString ScanAnimationSource(UObject* AnimationSource);
};
