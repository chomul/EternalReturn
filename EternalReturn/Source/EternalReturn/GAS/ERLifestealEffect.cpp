// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERLifestealEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

UERLifestealEffect::UERLifestealEffect()
{
	// Instant 여야 IncomingHealing 이 실행되고 PostGameplayEffectExecute 가 불린다.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Mod;
	Mod.Attribute = UERAttributeSet::GetIncomingHealingAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	// FSetByCallerFloat 은 태그를 받는 생성자가 없다 (GameplayEffect.h:247-249). 필드로 넣는다.
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ERTags::SetByCaller_HealAmount;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Mod);
}
