// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EternalReturn : ModuleRules
{
	public EternalReturn(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(new string[] { "EternalReturn" });

<<<<<<< HEAD
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks" });
=======
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput" });
>>>>>>> 6100863f0a6ea62466e1603dfe2b39fdbd5c6654
    }
}
