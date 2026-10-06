// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAbilitySystemComponent;

/**
 * 보호막 (F19-04 시셀라 E · Argument 68 S1).
 *
 * 양은 어트리뷰트 `Shield` 의 **베이스값**에 더한다 (지속 GE 모디파이어는 피해로 깎을 수 없어서).
 * 지속시간은 태그 GE(`State.Shielded`)가 센다 — 끝나면 남은 보호막을 0 으로.
 * 깎는 곳은 UERAttributeSet::PostGameplayEffectExecute (피해가 HP 로 가기 전).
 */
namespace ERShield
{
	/** [서버] 보호막 Amount 를 Duration 초 동안. Why = 로그용 (스킬 이름) */
	void Apply(UAbilitySystemComponent* ASC, float Amount, float Duration, const FString& Why);
}
