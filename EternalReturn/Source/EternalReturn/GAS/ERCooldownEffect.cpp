// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERCooldownEffect.h"
#include "GAS/ERGameplayTags.h"

UERCooldownEffect::UERCooldownEffect()
{
	// ⚠ 기본값은 Instant 다 (GameplayEffect.cpp:146). Instant 는 태그를 못 준다 -> 반드시 HasDuration.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// FSetByCallerFloat 은 태그를 받는 생성자가 없다 (GameplayEffect.h:247-249). 필드로 넣는다.
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ERTags::SetByCaller_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	// 모디파이어 없음. 어트리뷰트를 건드리지 않는다 — 존재 자체(+태그)가 쿨다운이다.
}
