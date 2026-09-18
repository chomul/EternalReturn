// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERLevelUpEffect.generated.h"

/**
 * 레벨업 1회분 스탯 성장 GE (F10-02). **Instant · Additive · SetByCaller** — 레벨당 증가분을 BaseValue 에 더한다.
 *
 * ⭐⭐ **Instant 여야 한다.** 장비 GE(UEREquipmentEffect · Infinite) 와 정반대다:
 *   레벨 성장은 영구적이고 "기본 공격력" 의 일부라 추가 공격력(EvaluateBonus = Current − Base)에 **들어가면 안 된다**.
 *   Infinite 로 만들면 레벨 19 만큼의 공격력이 전부 추가분이 되어 BonusAPRatio 스킬이 폭주한다 (Docs/4_Argument/5).
 *   초기 스탯(F02-05)도 Instant 로 Base 를 세팅했다 — 같은 경로를 탄다.
 *
 * ⚠ 모디파이어 순서: **MaxHP 가 HP 보다 앞**. Instant 는 배열 순서대로 즉시 적용되므로(GameplayEffect.cpp:3004)
 *   뒤집히면 HP 클램프가 옛 MaxHP 로 잘라 버린다 (ERAttributeInit 과 같은 이유).
 *
 * 적용은 UERGrowthComponent::ApplyLevelGrowth — 레벨마다 한 번.
 */
UCLASS()
class UERLevelUpEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERLevelUpEffect();
};
