// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERDownedEffects.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

namespace
{
	FGameplayModifierInfo SetByCallerModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;
		FGameplayModifierInfo Mod;
		Mod.Attribute = Attribute;
		Mod.ModifierOp = Op;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Mod;
	}
}

UERDownedEffect::UERDownedEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	// 원작 "이동 속도가 1 로" — 둔화 · 이속 버프를 다 덮는다 (Override)
	Modifiers.Add(SetByCallerModifier(UERAttributeSet::GetMoveSpeedAttribute(), EGameplayModOp::Override, ERTags::SetByCaller_Downed_MoveSpeed));
}

UERDownedBleedEffect::UERDownedBleedEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false;   // 쓰러진 순간이 아니라 1초 뒤부터
	// ⭐ HP 를 직접 깎는다 (피해 통로 IncomingDamage 가 아니다) — 피해로 보내면 받은 피해 숙련도 · 전투 상태 · 피해 감소가 매초 끼어든다.
	//   0 이 되면 어트리뷰트셋의 HP 직접 변경 분기가 사망 알림을 쏜다.
	Modifiers.Add(SetByCallerModifier(UERAttributeSet::GetHPAttribute(), EGameplayModOp::Additive, ERTags::SetByCaller_Downed_Bleed));
}

UERDeadEffect::UERDeadEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}
