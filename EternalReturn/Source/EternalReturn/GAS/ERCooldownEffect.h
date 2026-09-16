// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERCooldownEffect.generated.h"

/**
 * 모든 스킬이 공유하는 **유일한** 쿨다운 GE.
 *
 * ⭐ 길이는 SetByCaller.Cooldown 으로 받는다. **스킬 가속은 여기서 계산하지 않는다** —
 *   UERGameplayAbility::ApplyCooldown 이 `기본 × 100 / (100 + SkillHaste)` 를 계산해서 넣는다.
 *   근거: Docs/4_Argument/16_쿨다운_가속환산_위치.md (방안 A)
 *
 * ⭐ 이 GE 는 태그를 부여하지 않는다. "어느 슬롯의 쿨인가" 는 어빌리티가
 *   스펙의 DynamicGrantedTags 로 Cooldown.Slot.* 를 심어 표현한다.
 *   그래서 슬롯·스킬마다 GE 를 따로 만들지 않아도 된다.
 *
 * ⚠ 네이티브인 이유: UERSlowEffect 와 같다 — 기획자가 만질 값이 0개다.
 *   쿨다운 수치는 UERSkillData.Cooldowns 에 있다.
 */
UCLASS()
class UERCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERCooldownEffect();
};
