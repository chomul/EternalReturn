// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/ERGameplayAbility.h"
#include "ERPassiveAbility.generated.h"

struct FGameplayEventData;

/**
 * 패시브 — 부여되면 서버에서 바로 켜져 **계속 듣는** 어빌리티 (F19-02 매그너스 P 근성 · Argument 61 E1).
 * Lyra 의 ActivationPolicy OnSpawn 과 같은 방식 (`LyraGameplayAbility.h:46` · TryActivateAbilityOnSpawn).
 *
 * 지금 듣는 것: `Event.Hit.Dealt` (어트리뷰트셋이 피해가 들어간 순간 가해자에게 보낸다) → 자기 DA 조각의 `OnHitDealt`.
 * 쿨다운 · 시전 · 애니 · 차단 태그 없음 (기절해도 근성은 쌓인다 — 적중은 이미 일어난 일).
 * 스킬 DA: AbilityClass = 이 클래스 · SlotTag = Ability.Slot.P · InitialLevel ≥ 1 (0 이면 켜지지 않는다).
 */
UCLASS()
class ETERNALRETURN_API UERPassiveAbility : public UERGameplayAbility
{
	GENERATED_BODY()

public:
	UERPassiveAbility();

protected:
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnHitDealt(FGameplayEventData Payload);
};
