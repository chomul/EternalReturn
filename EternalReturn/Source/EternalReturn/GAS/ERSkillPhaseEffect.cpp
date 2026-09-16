// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERSkillPhaseEffect.h"
#include "GAS/ERGameplayTags.h"

UERSkillPhaseEffect::UERSkillPhaseEffect()
{
	// Instant 는 태그를 못 준다 -> HasDuration.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ERTags::SetByCaller_PhaseDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	// 모디파이어 없음. 태그도 없음 — 어빌리티가 동적으로 심는다.
}
