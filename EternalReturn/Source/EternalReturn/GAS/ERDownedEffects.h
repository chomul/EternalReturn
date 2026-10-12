// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERDownedEffects.generated.h"

/**
 * 빈사 · 사망 상태 GE (Argument 75 A1). 태그는 적용하는 쪽(AERPlayerState)이 Spec 의 DynamicGrantedTags 로 심는다 — 쿨다운 · 페이즈 GE 와 같은 패턴.
 * ⭐ 부여 태그는 Mixed 복제에서 모두에게 간다 → 각 머신의 연출 컴포넌트가 태그를 구독해 자세 · 사망 포즈를 튼다 (새 복제 없음).
 */

/** 빈사 — Infinite · 이동 속도 Override (SetByCaller.Downed.MoveSpeed). 태그 State.Downed (AnimBP 빈사 전용 상태 · Argument 76). 받는 피해 감소는 어트리뷰트셋이 (레벨식이라) */
UCLASS()
class UERDownedEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERDownedEffect();
};

/** 빈사 체력 감소 — Infinite · 1초마다 HP += SetByCaller.Downed.Bleed (음수). 0 이 되면 어트리뷰트셋이 사망 알림 */
UCLASS()
class UERDownedBleedEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERDownedBleedEffect();
};

/** 사망 — Infinite · 효과 없음. 태그 State.Dead · State.Block.Movement · State.Untargetable (+ 빈사에서 죽었으면 State.Dead.FromDowned — 사망 애니 down_dead) */
UCLASS()
class UERDeadEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERDeadEffect();
};
