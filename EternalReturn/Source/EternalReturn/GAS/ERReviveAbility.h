// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ERReviveAbility.generated.h"

class AERPlayerState;

/**
 * 아군 소생 (F14 · Argument 75 R1) — 빈사 아군 곁에서 ReviveSeconds 채널 → 그 아군이 일어선다.
 *
 * ⭐ 스킬 DA 가 없는 공용 행동이라 UERGameplayAbility(스킬 데이터 기반)가 아니다. 모든 실험체에게 PlayerState 가 준다.
 * 발동: PC 가 빈사 아군 우클릭 → Server RPC → 서버가 ASC 에 `Event.Revive.Request` (대상 = 아군 폰) → 이 어빌리티 (서버만 · 예측 없음 §8)
 * 끊김 (사용자 2026-10-08): **움직임** (PC 이동 요청이 `Ability.Revive` 로 취소) · **CC** (`State.CC.*` 가 붙으면). 피해만으로는 안 끊긴다.
 *   그 밖: 대상이 빈사에서 벗어남 (사망 · 다른 사람이 먼저 소생) · 내가 쓰러짐 (빈사 진입이 어빌리티를 취소).
 * 소생 중 태그 `State.Reviving` (**복제 loose** — 모든 머신이 소생 애니 resurrect) — 스킬 · 평타가 막힌다 (UERGameplayAbility::CanActivateAbility).
 */
UCLASS()
class ETERNALRETURN_API UERReviveAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UERReviveAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 소생 가능한 대상인가 (같은 팀 · 다른 사람 · 빈사 · 거리) — PC 의 선판정과 서버가 같은 함수. 거리는 Slack(cm) 만큼 너그럽게 */
	static bool CanRevive(const AActor* Reviver, const AActor* TargetPawn, float SlackCm, FString* OutWhy = nullptr);

private:
	UFUNCTION()
	void OnChannelDone();
	UFUNCTION()
	void OnInterrupted();

	TWeakObjectPtr<AERPlayerState> TargetPS;
	TWeakObjectPtr<AActor> TargetPawn;
	bool bCompleted = false;
	/** State.Reviving 복제 loose 태그를 붙였나 — 뗄 때 짝을 맞춘다 */
	bool bTagged = false;
};
