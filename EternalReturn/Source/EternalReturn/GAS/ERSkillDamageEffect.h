// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERSkillDamageEffect.generated.h"

/**
 * 스킬 피해 GE. **Instant, Execution = UERDamageExecution, 그 외 아무것도 없음.**
 *
 * ⭐ 계수(고정 피해 · 공격력 · 추가 공격력 · 스킬 증폭)는 SetByCaller(Data.Damage.*)로,
 *   채널(Damage.Type.Skill / True)과 형상(Damage.Shape.AoE)은 **동적 애셋 태그**로 어빌리티가 넣는다
 *   (UERGameplayAbility::ApplySkillDamage). F03 디버그가 런타임에 만들던 GE_ERDebugDamage 의 정식 버전.
 *
 * ⚠ 네이티브인 이유: 기획자가 만질 값이 0개다 — 수치는 전부 UERSkillData 에 있다 (Argument 9 · 16 과 같은 논리).
 *   "스킬마다 GE 애셋" 은 30개 애셋에 같은 Execution 하나를 반복해 넣는 일이다.
 */
UCLASS()
class UERSkillDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERSkillDamageEffect();
};
