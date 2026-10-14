// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERCombatSettings.generated.h"

/** 전투 상태 (F11-04 에서 만듦 — 무기 교체 판정. 비전투 재생 · 귀환도 이걸 쓴다). Project Settings > Game > ER Combat. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Combat"))
class UERCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UERCombatSettings& Get() { return *GetDefault<UERCombatSettings>(); }

	/** 피해를 주거나 받은 뒤 전투 상태(State.InCombat)가 유지되는 초. 다시 피해가 오가면 갱신. ⚠ 자체 결정값 — 원작 (미확인). */
	UPROPERTY(config, EditAnywhere, Category = "전투 상태", meta = (ClampMin = "0.1"))
	float CombatStateSeconds = 5.f;

	// ── 빈사 · 소생 (Argument 75 · 역기획서 매치진행 §4.9) ──

	/** 빈사가 되면 체력을 이 값으로 (최대 체력보다 크면 최대 체력). 원작 400 [확인 — 나무위키 가이드]. */
	UPROPERTY(config, EditAnywhere, Category = "빈사", meta = (ClampMin = "1"))
	float DownedHP = 400.f;

	/** 빈사 이동 속도 (m/s · 다른 이속 효과를 덮는다). 원작 1 [확인]. */
	UPROPERTY(config, EditAnywhere, Category = "빈사", meta = (ClampMin = "0"))
	float DownedMoveSpeed = 1.f;

	/** 빈사 받는 피해 감소 = 기본 + 레벨당 × 레벨 (0~1). 원작 50% + 레벨 × 1.5% [확인]. */
	UPROPERTY(config, EditAnywhere, Category = "빈사", meta = (ClampMin = "0", ClampMax = "1"))
	float DownedDamageReductionBase = 0.5f;
	UPROPERTY(config, EditAnywhere, Category = "빈사", meta = (ClampMin = "0", ClampMax = "1"))
	float DownedDamageReductionPerLevel = 0.015f;

	/** 빈사 초당 체력 감소. 원작 1 [확인]. */
	UPROPERTY(config, EditAnywhere, Category = "빈사", meta = (ClampMin = "0"))
	float DownedBleedPerSecond = 1.f;

	// ── 빈사 스킬 (F14 S4 · Argument 79) ──

	/** 모든 실험체가 받는 빈사 스킬 DA (Q 분열 폭발 · W 집념 · R 자가 제세동). 원본 = Docs/3_EditorTasks/Data/Skills/Downed.json → ER.Skill.ImportJson Downed */
	UPROPERTY(config, EditAnywhere, Category = "빈사 스킬")
	TArray<TSoftObjectPtr<class UERSkillData>> DownedSkills;

	/** 집념(W) 중 빈사 이속 증가 비율 — 빈사 이속 × (1 + 이 값). 원작 +100% [확인 — 나무위키 가이드 · 사용자 표 2026-10-12] */
	UPROPERTY(config, EditAnywhere, Category = "빈사 스킬", meta = (ClampMin = "0"))
	float DownedObstinacyMoveSpeedBonus = 1.f;

	// ── 전술 스킬 F (Argument 80) ──

	/** 고를 수 있는 전술 스킬 DA. 캐릭터 선택 UI 는 이 **순서(번호)** 로 고른다 — 순서를 바꾸면 번호가 바뀐다. 원본 = Docs/3_EditorTasks/Data/Skills/Tactical.json → ER.Skill.ImportJson Tactical */
	UPROPERTY(config, EditAnywhere, Category = "전술 스킬")
	TArray<TSoftObjectPtr<class UERSkillData>> TacticalSkills;

	/** 고르기 전 기본 (TacticalSkills 번호). -1 = F 없음. ⚠ 원작 기본값 (미확인) — 블링크 */
	UPROPERTY(config, EditAnywhere, Category = "전술 스킬", meta = (ClampMin = "-1"))
	int32 DefaultTacticalSkill = 0;

	/** 아군 소생에 걸리는 초. ⚠ 자체 결정값 — 원작 (미확인). */
	UPROPERTY(config, EditAnywhere, Category = "소생", meta = (ClampMin = "0.1"))
	float ReviveSeconds = 5.f;

	/** 소생할 수 있는 거리 (m · 캡슐 가장자리 사이 아님 — 중심 사이). ⚠ 자체 결정값. */
	UPROPERTY(config, EditAnywhere, Category = "소생", meta = (ClampMin = "0.5"))
	float ReviveRange = 2.f;

	/** 소생돼 일어선 뒤 체력 (최대 체력 비율). ⚠ 자체 결정값 — 자가 소생 25% [요약 출처] 에 맞춤. */
	UPROPERTY(config, EditAnywhere, Category = "소생", meta = (ClampMin = "0.01", ClampMax = "1"))
	float ReviveHPRatio = 0.25f;
};
