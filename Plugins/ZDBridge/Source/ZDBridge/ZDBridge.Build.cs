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
            "PaperZD"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AssetTools",
            "UnrealEd",
            "Paper2DEditor",
            "PaperZDEditor",
            "AssetRegistry",
            "Json"
        });
    }
}
