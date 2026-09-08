using UnrealBuildTool;

public class ZDBridge : ModuleRules
{
    public ZDBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Paper2D",
            "PaperZD",
            // 公共头里用到 FMetasoundFrontendLiteral，必须是 Public 依赖。
            "MetasoundFrontend"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AssetTools",
            "UnrealEd",
            "Paper2DEditor",
            "PaperZDEditor",
            "AssetRegistry",
            "Json",
            "JsonUtilities",
            "MetasoundEngine",
            "MetasoundEditor"
        });
    }
}
