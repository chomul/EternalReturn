// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/EREquipmentEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

const TArray<UEREquipmentEffect::FBinding>& UEREquipmentEffect::GetBindings()
{
	// 장비 역기획서 §6.1 옵션 중 F02 어트리뷰트에 있는 것 전부. 현재값(HP · VP) · 메타(Incoming*) · 모드 배율은 장비 대상이 아니다.
	// ⚠ 순서는 의미 없다 (태그로 매칭). 어트리뷰트를 새로 만들면 여기에 한 줄 추가 — 안 하면 그 옵션이 조용히 무시된다.
	static const TArray<FBinding> Bindings = {
		{ UERAttributeSet::GetMaxHPAttribute(),            ERTags::SetByCaller_MaxHP },
		{ UERAttributeSet::GetHPRegenAttribute(),          ERTags::SetByCaller_HPRegen },
		{ UERAttributeSet::GetMaxVPAttribute(),            ERTags::SetByCaller_MaxVP },
		{ UERAttributeSet::GetVPRegenAttribute(),          ERTags::SetByCaller_VPRegen },
		{ UERAttributeSet::GetAttackPowerAttribute(),      ERTags::SetByCaller_AttackPower },
		{ UERAttributeSet::GetDefenseAttribute(),          ERTags::SetByCaller_Defense },
		{ UERAttributeSet::GetAttackSpeedAttribute(),      ERTags::SetByCaller_AttackSpeed },
		{ UERAttributeSet::GetMoveSpeedAttribute(),        ERTags::SetByCaller_MoveSpeed },
		{ UERAttributeSet::GetSightAttribute(),            ERTags::SetByCaller_Sight },
		{ UERAttributeSet::GetAttackRangeAttribute(),      ERTags::SetByCaller_AttackRange },
		{ UERAttributeSet::GetCritChanceAttribute(),       ERTags::SetByCaller_CritChance },
		{ UERAttributeSet::GetCritDamageUpAttribute(),     ERTags::SetByCaller_CritDamageUp },
		{ UERAttributeSet::GetSkillAmpAttribute(),         ERTags::SetByCaller_SkillAmp },
		{ UERAttributeSet::GetBasicAtkAmpAttribute(),      ERTags::SetByCaller_BasicAtkAmp },
		{ UERAttributeSet::GetDefPenPercentAttribute(),    ERTags::SetByCaller_DefPenPercent },
		{ UERAttributeSet::GetDefPenFlatAttribute(),       ERTags::SetByCaller_DefPenFlat },
		{ UERAttributeSet::GetDamageUpAttribute(),         ERTags::SetByCaller_DamageUp },
		{ UERAttributeSet::GetSkillHasteAttribute(),       ERTags::SetByCaller_SkillHaste },
		{ UERAttributeSet::GetDamageDownAttribute(),       ERTags::SetByCaller_DamageDown },
		{ UERAttributeSet::GetSlowResistAttribute(),       ERTags::SetByCaller_SlowResist },
		{ UERAttributeSet::GetCCResistAttribute(),         ERTags::SetByCaller_CCResist },
		{ UERAttributeSet::GetLifestealAttribute(),        ERTags::SetByCaller_Lifesteal },
		{ UERAttributeSet::GetOmniLifestealAttribute(),    ERTags::SetByCaller_OmniLifesteal },
		{ UERAttributeSet::GetHealAmpAttribute(),          ERTags::SetByCaller_HealAmp },
		{ UERAttributeSet::GetOutOfCombatRegenAttribute(), ERTags::SetByCaller_OutOfCombatRegen },
	};
	return Bindings;
}

UEREquipmentEffect::UEREquipmentEffect()
{
	// ⭐⭐ Infinite. 절대 Instant 로 바꾸지 않는다 (클래스 주석).
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	for (const FBinding& B : GetBindings())
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = B.Attribute;
		Mod.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = B.Tag;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(Mod);
	}
}
