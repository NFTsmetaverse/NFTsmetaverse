using UnrealBuildTool;

public class ChainOfWitnesses : ModuleRules
{
	public ChainOfWitnesses(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"UMG",
			"Slate",
			"SlateCore",
			// UChainContentSettings: puts the content tables on a Project Settings
			// page instead of requiring a placed asset or a Blueprint.
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
