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

    // Unbinds sequences from their PaperZD AnimationSource without touching the assets.
    //
    // PaperZD 2.2 keeps no SupportedAnimations array on the source: the editor's list is an
    // asset-registry query over sequences whose AnimSource points at it. So "removing an entry
    // from the source" means clearing that pointer on the sequence itself.
    //
    // The assets are deliberately left on disk. A sequence that drifted into the wrong source
    // usually still holds real artwork, and deleting it because it looked out of place would
    // destroy content that is merely mis-filed.
    //
    // Returns JSON: { protocolName, protocolVersion, detachedCount, items: [ { objectPath,
    // detached, assetClass, previousSource, error } ] }
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static FString DetachSequencesFromAnimationSource(const TArray<FString>& SequenceObjectPaths);

    /**
     * 按行写入数据表，只覆盖 RowJson 里出现的字段，其余字段保持原样。
     *
     * 不能用 UDataTable::FillFromJSONString 代替：FSkillData2D 自带一个 Name 属性，
     * 和数据表的行名字段同名，整表回灌会把每一行的行名覆盖成技能名字数组，
     * 27 行护援技会一次性全废。这里逐行取结构体内存、按字段覆盖、再写回。
     *
     * FText 字段会尽量沿用该位置原有的命名空间和键，只换文本；
     * 原来没有键（或是新行）时才生成新的，避免每次同步都刷掉本地化条目。
     *
     * @return JSON：{ ok, tableObjectPath, rowName, created, writtenFields[], error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|DataTable", CallInEditor)
    static FString UpsertDataTableRow(const FString& TableObjectPath, FName RowName, const FString& RowJson);

    /**
     * 把 JSON 里出现的字段覆盖进 Owner 上某个结构体属性，其余字段保持原样。
     *
     * 角色蓝图的 SkillSlot1..3 是 FSkillData2D，里面的技能名字和介绍是 FText。
     * 走 Python 的 set_editor_property 只能塞进文化无关文本，本地化条目会被降级；
     * 这里和数据表用同一套写入逻辑，键沿用原位置的，改的只是文本。
     *
     * @return JSON：{ ok, structProperty, writtenFields[], error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Assets", CallInEditor)
    static FString ApplyJsonToStructProperty(UObject* Owner, FName StructPropertyName, const FString& Json);

    /**
     * 读一行数据表，返回这一行的完整 JSON。
     *
     * 不能用 UDataTable::GetTableAsJSON（Python 侧的 export_to_json_string）代替：
     * 那个导出器把行名字段（默认就叫 "Name"）当作 FieldToSkip 传给 WriteStruct，
     * 而 FSkillData2D 恰好也有一个 Name 属性，于是每一行的「技能名字」导出来
     * 永远是空数组。拿它做比对，这个字段会被永远判成待写入，写进去了也看不出变化。
     *
     * 这里用 FJsonObjectConverter 读，和 UpsertDataTableRow 的写入走同一套转换，
     * 读回来的形状和写进去的形状天然对得上。
     *
     * @return JSON：{ ok, found, rowName, row: {...}, error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|DataTable", CallInEditor)
    static FString ReadDataTableRow(const FString& TableObjectPath, FName RowName);
};
