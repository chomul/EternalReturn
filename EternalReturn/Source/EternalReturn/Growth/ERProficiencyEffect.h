// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERProficiencyEffect.generated.h"

/**
 * 무기 숙련도 증폭 GE (F10-04). **Infinite · Additive · SetByCaller** — SkillAmp / BasicAtkAmp 둘 다 모디파이어로 두고 한쪽에만 값을 넣는다.
 *
 * ⭐ **Infinite 여야 한다** — 장비 GE(UEREquipmentEffect) 와 같은 부류다: 무기를 바꾸면 사라지는 **조건부** 보너스라 CurrentValue 에만.
 *   레벨 성장(UERLevelUpEffect · Instant) 과 헷갈리지 않는다.
 * 레벨이 오르거나 무기가 바뀌면 **핸들 제거 후 재적용** (UERGrowthComponent::RefreshProficiencyBonus). 수동 차감 없음.
 */
UCLASS()
class UERProficiencyEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERProficiencyEffect();
};
