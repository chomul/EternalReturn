// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERWildlifeSettings.generated.h"

class AERWildlifeCharacter;
class UERWildlifeData;
class UERSkillData;

/** 야생동물 애셋 위치 · 스폰 클래스. Project Settings > Game > ER Wildlife. (다른 Settings 와 같은 이유로 DeveloperSettings) */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Wildlife"))
class UERWildlifeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	static const UERWildlifeSettings& Get() { return *GetDefault<UERWildlifeSettings>(); }

	/** 종 정의 애셋 폴더. 이름 규약 `DA_Wild_<이름>` — FindData("Bear") = <DataPath>/DA_Wild_Bear. 스폰표(03)는 애셋을 직접 참조하므로 이 규약은 디버그 · 이름 조회용. */
	UPROPERTY(config, EditAnywhere, Category = "야생동물")
	FString DataPath = TEXT("/Game/Wildlife");

	/** 스폰할 액터 클래스 (BP_ERWildlife — 캡슐 · 기본 메시 · InitStatsEffect 는 BP 가 든다). 종별 메시는 DA 가 덮는다. */
	UPROPERTY(config, EditAnywhere, Category = "야생동물")
	TSoftClassPtr<AERWildlifeCharacter> WildlifeClass;

	/**
	 * 공통 평타 (F12-04). 종 정의의 `Skills` 에 평타 슬롯(Ability.Slot.Attack) 스킬이 **없으면** 이것을 부여한다 (사용자 2026-09-24 "② 로").
	 * ⭐ 21종을 하나씩 만지지 않게 — 곰처럼 다른 평타가 필요한 종만 자기 DA 에 넣으면 그게 우선이다. 새 종도 자동으로 받는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "AI")
	TSoftObjectPtr<UERSkillData> DefaultAttackSkill;

	/**
	 * 재생성 때 변이로 나올 확률 (0~1) — 스폰 자리에 `MutantSpecies` 가 있을 때만. 첫 스폰은 항상 일반.
	 * 원작 확률은 (미확인) → **20% `[자체]`** (사용자 2026-09-24 "에디터에서 수정할 수 있게").
	 */
	UPROPERTY(config, EditAnywhere, Category = "스폰", meta = (ClampMin = "0", ClampMax = "1"))
	float MutantChance = 0.2f;

	// ── 전투 AI (F12-04 · Argument 35 A · 역기획서 §6.5) ──

	/** 어그로 한계 (m) — **자기 자리 기준 10m** [확인] (사용자 2026-09-24). 넘으면 귀환 · 회복. */
	UPROPERTY(config, EditAnywhere, Category = "AI", meta = (ClampMin = "1"))
	float LeashMeters = 10.f;

	/** 전투 중 판단 주기 (초). 대기 중엔 타이머가 없다 — 피격 이벤트로만 깨어난다. `[자체]` */
	UPROPERTY(config, EditAnywhere, Category = "AI", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.25f;

	/** 복제 빈도 — 대기 5 · 전투/귀환 20 (역기획서 §7.3 초기값 `[자체]` · 측정 후 조정). */
	UPROPERTY(config, EditAnywhere, Category = "AI", meta = (ClampMin = "1"))
	float IdleNetUpdateFrequency = 5.f;

	UPROPERTY(config, EditAnywhere, Category = "AI", meta = (ClampMin = "1"))
	float CombatNetUpdateFrequency = 20.f;

	/** 보스 복제 빈도 — 도먼시 없이 항상 (Task 05 · §7.3 초기값 `[자체]`). */
	UPROPERTY(config, EditAnywhere, Category = "AI", meta = (ClampMin = "1"))
	float BossNetUpdateFrequency = 30.f;

	/** 이름으로 종 정의 애셋 로드. 없으면 Error 로그 + nullptr. */
	static const UERWildlifeData* FindData(FName Name);
};
