#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetasoundFrontendLiteral.h"
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

    // Sets a MetaSound graph variable default on the frontend document.
    // The engine only exposes GetGraphVariableDefault to script; without a setter the toolbox
    // has to write through UMetasoundEditorGraphMemberDefault*Array::Defaults, which is declared
    // Transient and is therefore empty unless the asset was opened in the MetaSound editor.
    // Returns an empty string on success, or an "error: ..." message.
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|MetaSound", CallInEditor)
    static FString SetMetaSoundGraphVariableDefault(UObject* MetaSound, FName VariableName, const FMetasoundFrontendLiteral& DefaultLiteral);

    // Python-friendly diagnostic snapshot of all loaded PaperZD sequences bound to an AnimSource.
    // Returns a JSON string so the toolbox can compare stable asset facts without parsing editor UI state.
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static FString ScanAnimationSource(UObject* AnimationSource);

    // Deletes an action's legacy assets, clearing whatever still holds them.
    //
    // Script cannot do this reliably. EditorAssetLibrary::DeleteAsset refuses on any referencer;
    // ObjectTools::ForceDeleteObjects nulls hard references through FArchiveReplaceObjectRef but
    // cannot touch soft ones, and the project-wide sprite atlas holds every sprite through
    // TSoftObjectPtr in UPaperSpriteAtlas::AtlasSlots. Python also cannot tell a hard referencer
    // from a soft one: FindPackageReferencersForAsset reports no dependency kind.
    //
    // Deletes referencing classes before referenced ones (sequence -> flipbook -> sprite ->
    // texture), strips soft holders it knows about, force-deletes the rest, then verifies each
    // asset is really gone instead of trusting any return value.
    //
    // Returns JSON: { protocolName, protocolVersion, deletedCount, items: [ { objectPath,
    // deleted, assetClass, hardReferencers, softReferencers, action, error } ] }
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Assets", CallInEditor)
    static FString PurgeAssets(const TArray<FString>& ObjectPaths);
};
