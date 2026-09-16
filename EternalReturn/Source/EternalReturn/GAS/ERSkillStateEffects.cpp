// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERSkillStateEffects.h"
#include "GAS/ERGameplayTags.h"

namespace
{
	FGameplayEffectModifierMagnitude StateDuration()
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = ERTags::SetByCaller_StateDuration;
		return FGameplayEffectModifierMagnitude(SetByCaller);
	}
}

UERNextAttackBuffEffect::UERNextAttackBuffEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = StateDuration();
}

UERNextAttackBuffInfiniteEffect::UERNextAttackBuffInfiniteEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UERRecastWindowEffect::UERRecastWindowEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = StateDuration();
}
