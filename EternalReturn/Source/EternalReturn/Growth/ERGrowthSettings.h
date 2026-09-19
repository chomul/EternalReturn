// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERGrowthSettings.generated.h"

class UDataTable;

/**
 * 성장 테이블의 위치와 경험치 계수. Project Settings > Game > ER Growth. (ERItemSettings 와 같은 이유로 DeveloperSettings)
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Growth"))
class UERGrowthSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UERGrowthSettings& Get() { return *GetDefault<UERGrowthSettings>(); }

	/** 레벨업 필요 경험치 테이블 (`DT_LevelExp`). 행 구조 FERLevelExpRow · 행 수 + 1 = 최대 레벨. */
	UPROPERTY(config, EditAnywhere, Category = "레벨", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> LevelExpTable;

	/** 시작 스킬 포인트. ⭐ 원작 확인값 1 ("거의 모든 실험체는 최초 1개의 스킬 포인트를 받는다" — 나무위키 가이드). 실험체별 예외는 (미확인). */
	UPROPERTY(config, EditAnywhere, Category = "스킬 포인트", meta = (ClampMin = "0"))
	int32 StartingSkillPoints = 1;

	/** 실험체 처치 시 처치자가 받는 경험치. ⚠ 자체 결정값 — 원작 (미확인) (역기획서 §11). 어시스트 분배 없음 `[자체]`. */
	UPROPERTY(config, EditAnywhere, Category = "경험치", meta = (ClampMin = "0"))
	int32 PlayerKillExp = 500;

	// ── 무기 숙련도 (F10-04) ───────────────────────────────────
	// ⭐ 곡선 = Lv2 임계값 + 등차 (나무위키 가이드 "무기 숙련도" [확인] · 역기획서 §3.3). 테이블 없음.

	/** Lv.1 → 2 필요 숙련도 경험치. [확인] 230 */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도", meta = (ClampMin = "1"))
	int32 ProficiencyExpLv2 = 230;

	/** 레벨당 필요 경험치 증가. [확인] 270 → Lv N→N+1 = 230 + 270×(N−1) */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도", meta = (ClampMin = "0"))
	int32 ProficiencyExpStep = 270;

	/** 최대 숙련도 레벨. ⚠ 자체 결정값 — 원작 (미확인). D 해금 5 / 강화 10 · 15 가 그 안에 있으면 된다. */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도", meta = (ClampMin = "1"))
	int32 ProficiencyMaxLevel = 20;

	/** 실험체에게 피해 100당 숙련도 경험치. [확인] 63 */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도", meta = (ClampMin = "0"))
	float ProficiencyExpPerPlayerDamage100 = 63.f;

	/** 야생동물에게 피해 100당. [확인] 5 (대상 태그 Actor.Type.Wildlife) */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도", meta = (ClampMin = "0"))
	float ProficiencyExpPerWildlifeDamage100 = 5.f;

	/** 실험체 처치. [확인] 100 */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도", meta = (ClampMin = "0"))
	float ProficiencyExpPerPlayerKill = 100.f;

	/** 무기 제작 시 그 무기군 숙련도 — 등급 순서(일반 · 고급 · 희귀 · 영웅 · 전설 · 초월). [확인] 100/200/350/550/800 · 초월은 (미확인) → 800 `[자체]` */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도")
	TArray<float> CraftProficiencyExpByGrade = { 100.f, 200.f, 350.f, 550.f, 800.f, 800.f };

	/** 그 아이템을 처음 만들 때 배율 보너스. [확인] +25% */
	UPROPERTY(config, EditAnywhere, Category = "무기 숙련도", meta = (ClampMin = "0"))
	float FirstCraftBonus = 0.25f;
};
