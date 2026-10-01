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
            // 스킬 DA JSON 임포트 (ER.Skill.ImportJson · Argument 55 J1) — 엔진 기본 모듈
            PrivateDependencyModuleNames.Add("Json");
            // 스킬 임포트가 바꾼 DA 를 바로 저장 (새 DA 를 저장 안 하고 PIE 하면 클라로 못 보낸다 — 2026-10-01 두 번)
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
