#include "ZDBridgeLibrary.h"

#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "Misc/PackageName.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperSpriteFactory.h"
#include "PaperFlipbookFactory.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/PaperZDAnimSequence_Flipbook.h"
#include "AnimSequences/Sources/PaperZDAnimationSource.h"
#include "AnimSequences/PaperZDFlipbookAnimDataSource.h"
#include "UObject/UnrealType.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Editor.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "ObjectTools.h"
#include "UObject/UObjectGlobals.h"
#include "AssetRegistry/IAssetRegistry.h"


namespace
{
    UObject* CreateAsset(const FString& PackagePath, const FString& AssetName, UClass* Class, UFactory* Factory, FString& Error)
    {
        if (!Class || !Factory)
        {
            Error = TEXT("asset class or factory is unavailable");
            return nullptr;
        }
        UObject* Existing = StaticLoadObject(Class, nullptr, *(PackagePath / AssetName + TEXT(".") + AssetName));
        if (Existing)
        {
            return Existing;
        }
        UObject* Asset = FAssetToolsModule::GetModule().Get().CreateAsset(
            AssetName, PackagePath, Class, Factory, NAME_None);
        if (!Asset)
        {
            Error = FString::Printf(TEXT("failed to create %s/%s"), *PackagePath, *AssetName);
        }
        return Asset;
    }

    bool SetProperty(UObject* Object, const FName Name, const void* Value)
    {
        if (FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(Name) : nullptr)
        {
            if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
            {
                ObjectProperty->SetObjectPropertyValue_InContainer(Object, *static_cast<UObject* const*>(Value));
                return true;
            }
        }
        return false;
    }
}

UPaperSprite* UZDBridgeLibrary::CreatePaperSpriteFromTexture(UTexture2D* Texture, const FString& PackagePath, const FString& AssetName, FString& Error)
{
    if (!Texture)
    {
        Error = TEXT("texture is null");
        return nullptr;
    }
    UPaperSpriteFactory* Factory = NewObject<UPaperSpriteFactory>();
    Factory->InitialTexture = Texture;
    UPaperSprite* Sprite = Cast<UPaperSprite>(CreateAsset(PackagePath, AssetName, UPaperSprite::StaticClass(), Factory, Error));
    if (Sprite)
    {
        Sprite->MarkPackageDirty();
    }
    return Sprite;
}

UPaperFlipbook* UZDBridgeLibrary::CreatePaperFlipbookFromSprites(const TArray<UPaperSprite*>& Sprites, const TArray<int32>& FrameRuns, float FramesPerSecond, const FString& PackagePath, const FString& AssetName, FString& Error)
{
    UPaperFlipbookFactory* Factory = NewObject<UPaperFlipbookFactory>();
    UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(CreateAsset(PackagePath, AssetName, UPaperFlipbook::StaticClass(), Factory, Error));
    if (!Flipbook)
    {
        return nullptr;
    }
    TArray<FPaperFlipbookKeyFrame> KeyFrames;
    for (int32 Index = 0; Index < Sprites.Num(); ++Index)
    {
        FPaperFlipbookKeyFrame& Key = KeyFrames.AddDefaulted_GetRef();
        Key.Sprite = Sprites[Index];
        Key.FrameRun = FrameRuns.IsValidIndex(Index) ? FMath::Max(1, FrameRuns[Index]) : 1;
    }
    Flipbook->Modify();
    if (FFloatProperty* FpsProperty = FindFProperty<FFloatProperty>(Flipbook->GetClass(), TEXT("FramesPerSecond")))
    {
        FpsProperty->SetPropertyValue_InContainer(Flipbook, FMath::Max(0.01f, FramesPerSecond));
    }
    if (FArrayProperty* FramesProperty = FindFProperty<FArrayProperty>(Flipbook->GetClass(), TEXT("KeyFrames")))
    {
        FScriptArrayHelper Helper(FramesProperty, FramesProperty->ContainerPtrToValuePtr<void>(Flipbook));
        Helper.EmptyAndAddValues(KeyFrames.Num());
        for (int32 Index = 0; Index < KeyFrames.Num(); ++Index)
        {
            FramesProperty->Inner->CopyCompleteValue(Helper.GetRawPtr(Index), &KeyFrames[Index]);
        }
    }
    Flipbook->MarkPackageDirty();
    return Flipbook;
}

UPaperZDAnimSequence* UZDBridgeLibrary::CreatePaperZDSequence(UPaperFlipbook* Flipbook, UObject* AnimationSource, const FString& PackagePath, const FString& AssetName, FString& Error)
{
    if (!Flipbook)
    {
        Error = TEXT("flipbook is null");
        return nullptr;
    }
    if (!AnimationSource)
    {
        Error = TEXT("animation source is null; PaperZD sequence creation requires an AnimMaps source");
        return nullptr;
    }
    UClass* SequenceClass = LoadClass<UPaperZDAnimSequence>(nullptr, TEXT("/Script/PaperZD.PaperZDAnimSequence_Flipbook"));
    UClass* FactoryClass = LoadClass<UFactory>(nullptr, TEXT("/Script/PaperZDEditor.PaperZDAnimSequenceFactory"));
    UFactory* Factory = FactoryClass ? NewObject<UFactory>(GetTransientPackage(), FactoryClass) : nullptr;
    if (!Factory || !SequenceClass)
    {
        Error = TEXT("PaperZD sequence class or factory is unavailable");
        return nullptr;
    }
    if (AnimationSource)
    {
        FProperty* Target = Factory->GetClass()->FindPropertyByName(TEXT("TargetAnimSource"));
        if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Target))
        {
            ObjectProperty->SetObjectPropertyValue_InContainer(Factory, AnimationSource);
        }
    }
    UPaperZDAnimSequence* Sequence = Cast<UPaperZDAnimSequence>(CreateAsset(PackagePath, AssetName, SequenceClass, Factory, Error));
    UPaperZDAnimSequence_Flipbook* FlipbookSequence = Cast<UPaperZDAnimSequence_Flipbook>(Sequence);
    if (!FlipbookSequence)
    {
        Error = TEXT("created asset is not a PaperZD flipbook sequence");
        return nullptr;
    }
    FlipbookSequence->Modify();
    if (FArrayProperty* AnimDataProperty = FindFProperty<FArrayProperty>(FlipbookSequence->GetClass(), TEXT("AnimData")))
    {
        FScriptArrayHelper Helper(AnimDataProperty, AnimDataProperty->ContainerPtrToValuePtr<void>(FlipbookSequence));
        Helper.EmptyAndAddValues(1);
        if (FStructProperty* StructProperty = CastField<FStructProperty>(AnimDataProperty->Inner))
        {
            if (FObjectProperty* AnimationProperty = FindFProperty<FObjectProperty>(StructProperty->Struct, TEXT("Animation")))
            {
                AnimationProperty->SetObjectPropertyValue_InContainer(Helper.GetRawPtr(0), Flipbook);
            }
        }
    }
    FlipbookSequence->MarkPackageDirty();
    return Sequence;
}

bool UZDBridgeLibrary::AddOrUpdateSupportedAnimation(UObject* AnimationSource, FName AnimationName, UPaperZDAnimSequence* Sequence, bool& Created, FString& Error)
{
    Created = false;
    if (!AnimationSource || !Sequence)
    {
        Error = TEXT("animation source or sequence is null");
        return false;
    }

    // PaperZD 2.2 does not store a SupportedAnimations array on the
    // AnimationSource. The editor's Supported Animations list is an asset
    // browser filtered by the AnimSource asset-registry tag. Creating a
    // sequence with TargetAnimSource set is therefore the native '+' action.
    UPaperZDAnimationSource* Source = Cast<UPaperZDAnimationSource>(AnimationSource);
    if (!Source)
    {
        Error = FString::Printf(TEXT("object is not a PaperZD AnimationSource: %s"), *AnimationSource->GetClass()->GetName());
        return false;
    }
    UE_LOG(LogTemp, Log, TEXT("ZDBridge AddOrUpdateSupportedAnimation: source=%s sequence=%s current=%s"), *Source->GetPathName(), *Sequence->GetPathName(), Sequence->GetAnimSource() ? *Sequence->GetAnimSource()->GetPathName() : TEXT("<none>"));
    if (Sequence->GetAnimSource() != Source)
    {
        Sequence->Modify();
        Sequence->SetAnimSource(Source);
        if (Sequence->GetAnimSource() != Source)
        {
            Error = TEXT("PaperZD sequence AnimSource could not be assigned");
            return false;
        }
        Sequence->MarkPackageDirty();
        Created = true;
    }
    return true;
}


FString UZDBridgeLibrary::SetMetaSoundGraphVariableDefault(UObject* MetaSound, FName VariableName, const FMetasoundFrontendLiteral& DefaultLiteral)
{
    if (!MetaSound)
    {
        return TEXT("error: MetaSound is null");
    }

    TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface(MetaSound);
    if (!DocumentInterface)
    {
        return FString::Printf(TEXT("error: object is not a MetaSound document: %s"), *MetaSound->GetClass()->GetName());
    }

    UMetaSoundEditorSubsystem* EditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>() : nullptr;
    if (!EditorSubsystem)
    {
        return TEXT("error: MetaSound editor subsystem is unavailable");
    }

    EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
    UMetaSoundBuilderBase* Builder = EditorSubsystem->FindOrBeginBuilding(DocumentInterface, BuilderResult);
    if (!Builder || BuilderResult != EMetaSoundBuilderResult::Succeeded)
    {
        return FString::Printf(TEXT("error: no document builder for %s"), *MetaSound->GetPathName());
    }

    MetaSound->Modify();
    if (!Builder->GetBuilder().SetGraphVariableDefault(VariableName, DefaultLiteral))
    {
        return FString::Printf(TEXT("error: graph variable is missing or not writable: %s"), *VariableName.ToString());
    }

    MetaSound->MarkPackageDirty();
    UE_LOG(LogTemp, Log, TEXT("ZDBridge SetMetaSoundGraphVariableDefault: %s.%s"), *MetaSound->GetPathName(), *VariableName.ToString());
    return FString();
}

FString UZDBridgeLibrary::ScanAnimationSource(UObject* AnimationSource)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Sequences;
    int32 ExcludedGeneratedSequenceCount = 0;
    if (!AnimationSource)
    {
        Root->SetStringField(TEXT("error"), TEXT("animation source is null"));
    }
    else
    {
        UPaperZDAnimationSource* Source = Cast<UPaperZDAnimationSource>(AnimationSource);
        if (!Source)
        {
            Root->SetStringField(TEXT("error"), TEXT("object is not a PaperZD AnimationSource"));
        }
        else
        {
            Root->SetStringField(TEXT("protocolName"), TEXT("ZDBridge.AnimationSourceScan"));
            Root->SetNumberField(TEXT("protocolVersion"), 3);
            Root->SetStringField(TEXT("animationSource"), Source->GetPathName());
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            TArray<FAssetData> Assets;
            AssetRegistryModule.Get().GetAssetsByClass(UPaperZDAnimSequence::StaticClass()->GetClassPathName(), Assets, true);
            for (const FAssetData& Asset : Assets)
            {
                UPaperZDAnimSequence* Sequence = Cast<UPaperZDAnimSequence>(Asset.GetAsset());
                if (!Sequence || Sequence->GetAnimSource() != Source)
                {
                    continue;
                }
                // AnimSequences 是工具箱同步生成的正式序列目录，不能整体排除。
                // 之前把该目录当成编辑器辅助资产排除，导致同步后复扫永远看不到 Sk1。
                TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("assetPath"), Sequence->GetPathName());
                Item->SetStringField(TEXT("assetClass"), Sequence->GetClass()->GetName());
                Item->SetStringField(TEXT("name"), Sequence->GetName());
                if (const UPaperZDAnimSequence_Flipbook* FlipbookSequence = Cast<UPaperZDAnimSequence_Flipbook>(Sequence))
                {
                    const TArray<FPaperZDFlipbookAnimDataSource>& Data = FlipbookSequence->GetAnimDataSource();
                    Item->SetNumberField(TEXT("dataSourceCount"), Data.Num());
                    if (Data.Num() > 0 && Data[0].Animation)
                    {
                        const UPaperFlipbook* Flipbook = Data[0].Animation;
                        Item->SetStringField(TEXT("flipbookPath"), Flipbook->GetPathName());
                        Item->SetNumberField(TEXT("frameCount"), Flipbook->GetNumKeyFrames());
                        Item->SetNumberField(TEXT("framesPerSecond"), Flipbook->GetFramesPerSecond());
                        TArray<TSharedPtr<FJsonValue>> Frames;
                        for (int32 FrameIndex = 0; FrameIndex < Flipbook->GetNumKeyFrames(); ++FrameIndex)
                        {
                            const FPaperFlipbookKeyFrame& KeyFrame = Flipbook->GetKeyFrameChecked(FrameIndex);
                            TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
                            Frame->SetNumberField(TEXT("index"), FrameIndex + 1);
                            Frame->SetNumberField(TEXT("frameRun"), KeyFrame.FrameRun);
                            if (KeyFrame.Sprite)
                            {
                                Frame->SetStringField(TEXT("spritePath"), KeyFrame.Sprite->GetPathName());
                                if (KeyFrame.Sprite->GetSourceTexture())
                                {
                                    Frame->SetStringField(TEXT("texturePath"), KeyFrame.Sprite->GetSourceTexture()->GetPathName());
                                }
                            }
                            Frames.Add(MakeShared<FJsonValueObject>(Frame));
                        }
                        Item->SetArrayField(TEXT("frames"), Frames);
                    }
                }
                Sequences.Add(MakeShared<FJsonValueObject>(Item));
            }
            Root->SetArrayField(TEXT("sequences"), Sequences);
            Root->SetNumberField(TEXT("sequenceCount"), Sequences.Num());
            Root->SetNumberField(TEXT("excludedGeneratedSequenceCount"), ExcludedGeneratedSequenceCount);
        }
    }
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

FString UZDBridgeLibrary::EnsureSequenceAnimationSource(UObject* AnimationSource, UPaperZDAnimSequence* Sequence)
{
    if (!AnimationSource || !Sequence)
    {
        return TEXT("error: animation source or sequence is null");
    }
    UPaperZDAnimationSource* Source = Cast<UPaperZDAnimationSource>(AnimationSource);
    if (!Source)
    {
        return FString::Printf(TEXT("error: object is not a PaperZD AnimationSource: %s"), *AnimationSource->GetClass()->GetName());
    }
    const bool bWasBound = Sequence->GetAnimSource() == Source;
    if (!bWasBound)
    {
        Sequence->Modify();
        Sequence->SetAnimSource(Source);
        if (Sequence->GetAnimSource() != Source)
        {
            return TEXT("error: PaperZD sequence AnimSource could not be assigned");
        }
        Sequence->MarkPackageDirty();
        return TEXT("created");
    }
    return TEXT("existing");
}


namespace
{
    /// 引用方先删、被引用方后删：序列 -> Flipbook -> Sprite -> 贴图。
    /// 反过来删，Unreal 会因为还有人引用而拒绝，留下一地孤儿。
    int32 PurgeRankOf(const UObject* Object)
    {
        if (!Object)
        {
            return 5;
        }
        const FString ClassName = Object->GetClass()->GetName();
        if (ClassName.Contains(TEXT("PaperZDAnimSequence"))) return 0;
        if (ClassName.Contains(TEXT("PaperFlipbook")))       return 1;
        if (ClassName.Contains(TEXT("PaperSprite")))         return 2;
        if (ClassName.Contains(TEXT("Texture")))             return 3;
        return 4;
    }

    FName PurgePackageNameOf(const FString& ObjectPath)
    {
        return FName(*FPackageName::ObjectPathToPackageName(ObjectPath));
    }

    bool PurgeAssetIsGone(const FString& ObjectPath)
    {
        // 只认事实：内存里没有、盘上也没有，才算删掉了。
        return FindObject<UObject>(nullptr, *ObjectPath) == nullptr &&
               !FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ObjectPath));
    }

    void PurgeCollectReferencers(
        IAssetRegistry& AssetRegistry,
        FName PackageName,
        TArray<FName>& OutHard,
        TArray<FName>& OutSoft)
    {
        using namespace UE::AssetRegistry;
        AssetRegistry.GetReferencers(PackageName, OutHard, EDependencyCategory::Package, EDependencyQuery::Hard);
        AssetRegistry.GetReferencers(PackageName, OutSoft, EDependencyCategory::Package, EDependencyQuery::Soft);
        OutHard.Remove(PackageName);
        OutSoft.Remove(PackageName);
    }
}

FString UZDBridgeLibrary::PurgeAssets(const TArray<FString>& ObjectPaths)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("protocolName"), TEXT("ZDBridge.PurgeAssets"));
    Root->SetNumberField(TEXT("protocolVersion"), 1);

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    struct FPurgeTarget
    {
        FString ObjectPath;
        UObject* Object = nullptr;
        FString AssetClass;
        int32 Rank = 5;
        bool bDeleted = false;
        FString Action;
        FString Error;
        TArray<FName> Hard;
        TArray<FName> Soft;
    };

    TArray<FPurgeTarget> Targets;
    for (const FString& ObjectPath : ObjectPaths)
    {
        if (ObjectPath.IsEmpty())
        {
            continue;
        }

        FPurgeTarget Target;
        Target.ObjectPath = ObjectPath;
        if (PurgeAssetIsGone(ObjectPath))
        {
            Target.bDeleted = true;
            Target.Action = TEXT("already-absent");
            Targets.Add(MoveTemp(Target));
            continue;
        }

        Target.Object = LoadObject<UObject>(nullptr, *ObjectPath);
        Target.AssetClass = Target.Object ? Target.Object->GetClass()->GetName() : FString();
        Target.Rank = PurgeRankOf(Target.Object);
        PurgeCollectReferencers(AssetRegistry, PurgePackageNameOf(ObjectPath), Target.Hard, Target.Soft);
        Targets.Add(MoveTemp(Target));
    }

    // 引用方排在被引用方前面。
    Targets.Sort([](const FPurgeTarget& A, const FPurgeTarget& B)
    {
        return A.Rank != B.Rank ? A.Rank < B.Rank : A.ObjectPath < B.ObjectPath;
    });

    // ForceDeleteObjects 一遍删不干净。实测：8 个一批只删掉 7 个，
    // 剩下那个单独重试一次就成功了 —— 尤其是源贴图已经不在的孤儿 Sprite。
    // 所以多跑几遍，直到某一遍不再有进展为止。
    auto CountRemaining = [&Targets]()
    {
        int32 Remaining = 0;
        for (const FPurgeTarget& Target : Targets)
        {
            if (!Target.bDeleted && !PurgeAssetIsGone(Target.ObjectPath))
            {
                ++Remaining;
            }
        }
        return Remaining;
    };

    const int32 MaxPasses = 3;
    for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
    {
        TArray<UObject*> Pending;
        for (const FPurgeTarget& Target : Targets)
        {
            if (Target.bDeleted || PurgeAssetIsGone(Target.ObjectPath))
            {
                continue;
            }

            // 每遍都重新解析：上一遍可能已经把对象销毁了，缓存的指针不能再碰。
            if (UObject* Object = LoadObject<UObject>(nullptr, *Target.ObjectPath))
            {
                Pending.AddUnique(Object);
            }
        }

        if (Pending.Num() == 0)
        {
            break;
        }

        const int32 BeforePass = CountRemaining();
        // 硬引用交给 ForceDeleteObjects 置空；bShowConfirmation=false 保持无人值守。
        ObjectTools::ForceDeleteObjects(Pending, false);
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

        if (CountRemaining() >= BeforePass)
        {
            // 这一遍毫无进展，再试也是白费。
            break;
        }
    }

    int32 DeletedCount = 0;
    TArray<TSharedPtr<FJsonValue>> Items;
    for (FPurgeTarget& Target : Targets)
    {
        if (!Target.bDeleted)
        {
            // 不信任何返回值，只复核资产是不是真的没了。
            Target.bDeleted = PurgeAssetIsGone(Target.ObjectPath);
            Target.Action = Target.bDeleted ? TEXT("force-deleted") : TEXT("failed");
            if (!Target.bDeleted)
            {
                Target.Hard.Reset();
                Target.Soft.Reset();
                PurgeCollectReferencers(AssetRegistry, PurgePackageNameOf(Target.ObjectPath), Target.Hard, Target.Soft);
                Target.Error = TEXT("still present after force delete");
            }
        }

        if (Target.bDeleted)
        {
            ++DeletedCount;
        }

        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("objectPath"), Target.ObjectPath);
        Item->SetBoolField(TEXT("deleted"), Target.bDeleted);
        Item->SetStringField(TEXT("assetClass"), Target.AssetClass);
        Item->SetStringField(TEXT("action"), Target.Action);
        Item->SetStringField(TEXT("error"), Target.Error);

        TArray<TSharedPtr<FJsonValue>> HardValues;
        for (const FName& Name : Target.Hard)
        {
            HardValues.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        TArray<TSharedPtr<FJsonValue>> SoftValues;
        for (const FName& Name : Target.Soft)
        {
            SoftValues.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        Item->SetArrayField(TEXT("hardReferencers"), HardValues);
        Item->SetArrayField(TEXT("softReferencers"), SoftValues);
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }

    Root->SetNumberField(TEXT("deletedCount"), DeletedCount);
    Root->SetNumberField(TEXT("requestedCount"), Targets.Num());
    Root->SetArrayField(TEXT("items"), Items);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}
