// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/ERWeaponRangeEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

UERWeaponRangeEffect::UERWeaponRangeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Mod;
	Mod.Attribute = UERAttributeSet::GetAttackRangeAttribute();
	Mod.ModifierOp = EGameplayModOp::Override;   // Base 교체. Instant 라 Current 의 Additive(장비) 는 유지된다

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ERTags::SetByCaller_AttackRange;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Mod);
}
