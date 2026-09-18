// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growth/ERProficiencyEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

UERProficiencyEffect::UERProficiencyEffect()
{
	// ⭐ Infinite. Instant 로 바꾸지 않는다 (클래스 주석).
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	const TPair<FGameplayAttribute, FGameplayTag> Mods[] = {
		{ UERAttributeSet::GetSkillAmpAttribute(),    ERTags::SetByCaller_SkillAmp },
		{ UERAttributeSet::GetBasicAtkAmpAttribute(), ERTags::SetByCaller_BasicAtkAmp },
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
