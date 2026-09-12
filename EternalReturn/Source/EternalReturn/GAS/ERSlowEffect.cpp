// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERSlowEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

UERSlowEffect::UERSlowEffect()
{
	// ⚠ Infinite 다. 지속시간은 이 GE 가 관리하지 않는다 —
	//   둔화 GE 애셋들이 각자의 지속시간을 갖고, 그것들이 모두 사라지면
	//   RecalculateSlow 가 이 GE 를 제거한다.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo Mod;
	Mod.Attribute = UERAttributeSet::GetMoveSpeedAttribute();

	// ⭐ Multiply 다. 배수를 받는다 (60% 둔화 -> 0.4).
	//   ⚠ 이 GE 는 **동시에 하나만** 존재해야 한다. 둘이 겹치면 Multiply 가
	//     합산되어 값이 틀린다(GameplayEffectAggregator.cpp:194-207).
	//     RecalculateSlow 가 항상 제거 후 적용하는 이유다.
	Mod.ModifierOp = EGameplayModOp::Multiplicitive;

	// FSetByCallerFloat 은 태그를 받는 생성자가 없다 (GameplayEffect.h:247-249). 필드로 넣는다.
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ERTags::SetByCaller_SlowMultiplier;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Mod);
}
