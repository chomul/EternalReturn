// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERCostEffect.generated.h"

/**
 * 스킬 코스트 GE. **Instant**, 한 어트리뷰트를 SetByCaller.Cost 만큼 뺀다.
 *
 * ⭐ 두 클래스인 이유: GE 모디파이어의 어트리뷰트는 클래스에 고정된다. "VP 아니면 HP" 를
 *   하나의 GE 로 표현하려면 안 쓰는 쪽 SetByCaller 를 0 으로 채워야 하는데, 그건 HP 에
 *   0 짜리 변경을 매번 흘리는 것이라 안 한다. 어빌리티가 CostType 으로 골라 쓴다
 *   (UERGameplayAbility::GetCostGameplayEffect).
 *
 * ⭐ 값은 UERGameplayAbility::ApplyCost 가 계산해서 넣는다 — 쿨다운과 같은 자리
 *   (Docs/4_Argument/16_쿨다운_가속환산_위치.md 방안 A 의 연장).
 *   HP 하한 1 도 거기서 `min(코스트, HP-1)` 로 깎는다. **어트리뷰트셋은 하한을 모른다** (F02-04).
 *
 * ⚠ HP 코스트는 IncomingDamage 를 타지 않는다 — 방어력·사망 경로와 무관한 순수 차감
 *   (역기획서 §3 "체력 코스트에 방어력 적용 안 함").
 */
UCLASS()
class UERVPCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERVPCostEffect();
};

UCLASS()
class UERHPCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERHPCostEffect();
};
