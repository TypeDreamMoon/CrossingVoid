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
