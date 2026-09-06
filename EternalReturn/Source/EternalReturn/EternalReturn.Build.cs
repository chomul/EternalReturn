// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EternalReturn : ModuleRules
{
	public EternalReturn(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(new string[] { "EternalReturn" });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks" });
    }
}
