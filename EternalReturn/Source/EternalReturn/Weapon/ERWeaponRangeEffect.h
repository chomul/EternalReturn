// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERWeaponRangeEffect.generated.h"

/**
 * 기본 공격 사거리 교체 GE (F11-01 · Docs/4_Argument/24 방안 B). **Instant · Override · SetByCaller.AttackRange** → BaseValue 를 계열 사거리로 바꾼다.
 *
 * ⭐ Infinite Override 가 아닌 이유: 애그리게이터는 Override 가 있으면 Additive 를 전부 무시한다 (GameplayEffectAggregator.cpp:60) —
 *   장비의 사거리 보너스(sniper_t1 +1.0)가 죽는다. Instant 로 Base 만 바꾸면 장비 Additive 는 Current 에 그대로 남는다.
 * 해제 때는 같은 GE 로 실험체 맨몸 값(CharacterData.BaseStats.AttackRange)을 다시 넣는다 — F11-02 가 한다.
 */
UCLASS()
class UERWeaponRangeEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERWeaponRangeEffect();
};
