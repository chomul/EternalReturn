// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERCostEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

namespace
{
	/** 어트리뷰트 하나를 SetByCaller.Cost 만큼 깎는 Additive 모디파이어. 부호는 GE 쪽(-1 계수)이 아니라 ApplyCost 가 음수로 넣는다. */
	FGameplayModifierInfo MakeCostModifier(const FGameplayAttribute& Attribute)
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = Attribute;
		Mod.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = ERTags::SetByCaller_Cost;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Mod;
	}
}

UERVPCostEffect::UERVPCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeCostModifier(UERAttributeSet::GetVPAttribute()));
}

UERHPCostEffect::UERHPCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeCostModifier(UERAttributeSet::GetHPAttribute()));
}
