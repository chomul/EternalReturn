// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EternalReturn : ModuleRules
{
	public EternalReturn(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(new string[] { "EternalReturn" });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks", "DeveloperSettings" });

        // 에디터 전용 — 임시 애셋 생성 명령(ER.Wild.ImportCSV)이 AssetCreated 알림에 쓴다. 게임 · 서버 타겟엔 안 들어간다
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("AssetRegistry");
        }
    }
}
