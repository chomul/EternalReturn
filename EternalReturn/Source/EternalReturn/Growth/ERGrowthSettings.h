// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERGrowthSettings.generated.h"

class UDataTable;

/**
 * 성장 테이블의 위치와 경험치 계수. Project Settings > Game > ER Growth. (ERItemSettings 와 같은 이유로 DeveloperSettings)
 *
 * ⭐ F10-05 (E18 A6~A10): **실험체 경험치의 유일한 입구는 숙련도 적립**이다 — 처치 경험치 · 야생동물 경험치 같은 별도 값은 없다.
 *   숙련도 6종에 쌓인 경험치 × LevelExpPerProficiencyExp 가 실험체 경험치 (Argument 26 B).
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

	/** 숙련도 경험치 1 당 실험체 경험치. (미확인) → 1:1 `[자체]`. 인게임에서 "무기 Lv.2(500) 시점의 실험체 경험치" 를 보면 바로 맞출 수 있다. */
	UPROPERTY(config, EditAnywhere, Category = "레벨", meta = (ClampMin = "0"))
	float LevelExpPerProficiencyExp = 1.f;

	// ── 숙련도 공통 (F10-05) ───────────────────────────────────

	/** 숙련도 필요 경험치 테이블 (`DT_ProficiencyExp`). 행 구조 FERProficiencyExpRow · 6열 · 행 수 + 1 = 숙련도 최대 레벨 (19행 → 20). 인게임 확인값. */
	UPROPERTY(config, EditAnywhere, Category = "숙련도", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> ProficiencyExpTable;

	// ── 무기 트랙 획득량 (F10-04 · E18 A8) ────────────────────

	/** 실험체에게 준 피해 100 당. [확인] 50 (나무위키) */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|무기", meta = (ClampMin = "0"))
	float WeaponExpPerPlayerDamage100 = 50.f;

	/** 야생동물에게 준 피해 100 당. ⚠ 자체 결정값 (원작 (미확인)) */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|무기", meta = (ClampMin = "0"))
	float WeaponExpPerWildlifeDamage100 = 5.f;

	/** 실험체 처치: 기본 + 적 레벨 × 계수. 원작 "적 레벨에 따라" [확인] · 값은 (미확인) `[자체]` */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|무기", meta = (ClampMin = "0"))
	float WeaponExpPerPlayerKillBase = 100.f;
	UPROPERTY(config, EditAnywhere, Category = "숙련도|무기", meta = (ClampMin = "0"))
	float WeaponExpPerPlayerKillPerLevel = 10.f;

	/** 무기 제작 시 그 무기군 숙련도 — 등급별 (일반 · 고급 · 희귀 · 영웅 · 전설 · 초월). [확인] 100~600 (E18 A8) · 등급 배분은 `[자체]` 선형 */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|무기")
	TArray<float> WeaponCraftExpByGrade = { 100.f, 200.f, 300.f, 400.f, 500.f, 600.f };

	/** 처음 만드는 아이템이면 +25%. [확인] */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|무기", meta = (ClampMin = "0"))
	float FirstCraftBonus = 0.25f;

	// ── 나머지 트랙 획득량 (F10-05) — 전부 자체 결정값. 원작 획득량 (미확인) ─────

	/** 방어: 받은 피해 100 당 `[자체]` */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|방어", meta = (ClampMin = "0"))
	float DefenseExpPerDamageTaken100 = 50.f;

	/** 사냥: 야생동물 처치 — 기본 + 동물 레벨 × 계수 `[자체]` (F12 가 부른다) */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|사냥", meta = (ClampMin = "0"))
	float HuntExpPerKillBase = 100.f;
	UPROPERTY(config, EditAnywhere, Category = "숙련도|사냥", meta = (ClampMin = "0"))
	float HuntExpPerKillPerLevel = 20.f;

	/** 제작: 모든 제작 — 등급별 `[자체]` (무기 제작은 무기 트랙에 **추가로** 들어간다) */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|제작")
	TArray<float> CraftExpByGrade = { 50.f, 100.f, 150.f, 200.f, 250.f, 300.f };

	/** 탐색: 상자 하나를 처음 열 때 `[자체]` */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|탐색", meta = (ClampMin = "0"))
	float SearchExpPerBox = 100.f;

	/** 이동: 100 m 당 `[자체]`. 1초마다 폰 위치 차이로 잰다 (순간이동 · 부활은 MoveSampleMaxMeters 넘으면 무시) */
	UPROPERTY(config, EditAnywhere, Category = "숙련도|이동", meta = (ClampMin = "0"))
	float MoveExpPer100m = 10.f;
	UPROPERTY(config, EditAnywhere, Category = "숙련도|이동", meta = (ClampMin = "1"))
	float MoveSampleMaxMeters = 20.f;
};
