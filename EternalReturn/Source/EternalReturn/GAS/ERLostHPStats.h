// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayModMagnitudeCalculation.h"
#include "ERLostHPStats.generated.h"

/**
 * 잃은 체력 비례 스탯 (시셀라 P 삶은 고통이에요 · Argument 68 P1).
 *
 * 값 = 최소 + (최대 − 최소) × 잃은 체력 비율 (1 − HP/MaxHP) — 정비례 [자체 · 사용자 2026-10-06].
 * ⭐ HP · MaxHP 를 **스냅샷 없이** 잡는다 — HP 가 바뀌면 GAS 가 이 GE 의 크기를 알아서 다시 계산한다
 *   (FActiveGameplayEffectsContainer::OnMagnitudeDependencyChange · GameplayEffect.cpp:3278). 직접 갱신 코드가 없다.
 * 최소 · 최대는 스펙의 SetByCaller (SetByCaller.LostHP.*) — 조각(UERSkillFragment_LostHPStats)이 레벨 값으로 넣는다.
 */
UCLASS(Abstract)
class UERLostHPStatCalc : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UERLostHPStatCalc();
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

protected:
	FGameplayTag MinTag;
	FGameplayTag MaxTag;

private:
	FGameplayEffectAttributeCaptureDefinition HPDef;
	FGameplayEffectAttributeCaptureDefinition MaxHPDef;
};

/** 체력 재생 몫 */
UCLASS()
class UERLostHPRegenCalc : public UERLostHPStatCalc
{
	GENERATED_BODY()
public:
	UERLostHPRegenCalc();
};

/** 스킬 증폭 몫 */
UCLASS()
class UERLostHPSkillAmpCalc : public UERLostHPStatCalc
{
	GENERATED_BODY()
public:
	UERLostHPSkillAmpCalc();
};

/** 상시 (패시브가 켜진 동안 · 레벨이 바뀌면 다시) */
UCLASS()
class UERLostHPStatsEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UERLostHPStatsEffect();
};

/** 시간 제한 — 같은 효과를 하나 더 (R 뒤 "패시브 효과 +100%" = 두 개가 더해진다) · 길이 SetByCaller.StateDuration */
UCLASS()
class UERLostHPStatsTimedEffect : public UERLostHPStatsEffect
{
	GENERATED_BODY()
public:
	UERLostHPStatsTimedEffect();
};
