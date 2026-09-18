// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growth/ERLevelUpEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

UERLevelUpEffect::UERLevelUpEffect()
{
	// ⭐⭐ Instant. 절대 Infinite 로 바꾸지 않는다 (클래스 주석).
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// ⚠ 순서 유지 — MaxHP 가 HP 보다 앞. 넷 다 FERCharStatGrowth 의 필드와 1:1 (HP 는 MaxHP 증가분을 그대로 받는다).
	const TPair<FGameplayAttribute, FGameplayTag> Mods[] = {
		{ UERAttributeSet::GetMaxHPAttribute(),       ERTags::SetByCaller_MaxHP },
		{ UERAttributeSet::GetHPAttribute(),          ERTags::SetByCaller_HP },
		{ UERAttributeSet::GetHPRegenAttribute(),     ERTags::SetByCaller_HPRegen },
		{ UERAttributeSet::GetAttackPowerAttribute(), ERTags::SetByCaller_AttackPower },
		{ UERAttributeSet::GetDefenseAttribute(),     ERTags::SetByCaller_Defense },
	};
	for (const auto& M : Mods)
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = M.Key;
		Mod.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = M.Value;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(Mod);
	}
}
