// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERReviveAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Combat/ERCombatSettings.h"
#include "Core/ERPlayerState.h"
#include "Core/ERTeamStatics.h"
#include "EternalReturn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GAS/ERGameplayTags.h"

UERReviveAbility::UERReviveAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;   // 예측 없음 (CLAUDE.md §8) — 결과(태그 · 체력)만 복제
	AbilityTags.AddTag(ERTags::Ability_Revive);             // 이동 요청이 이 태그로 취소한다 (AERPlayerController::ServerSetDestination)
	ActivationBlockedTags.AddTag(ERTags::State_Downed);
	ActivationBlockedTags.AddTag(ERTags::State_Dead);
	ActivationBlockedTags.AddTag(ERTags::State_Reviving);
	ActivationBlockedTags.AddTag(ERTags::State_CC);          // CC 중에는 시작도 못 한다 (끊기는 조건과 같은 축)
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = ERTags::Event_Revive_Request;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(Trigger);
	}
}

bool UERReviveAbility::CanRevive(const AActor* Reviver, const AActor* InTargetPawn, float SlackCm, FString* OutWhy)
{
	auto Fail = [OutWhy](const FString& Why) { if (OutWhy) { *OutWhy = Why; } return false; };
	const APawn* Target = Cast<APawn>(InTargetPawn);
	if (!Reviver || !Target)
	{
		return Fail(TEXT("대상 없음"));
	}
	if (Reviver == Target)
	{
		return Fail(TEXT("자기 자신"));
	}
	if (!ERTeamStatics::IsSameTeam(Reviver, Target))
	{
		return Fail(TEXT("같은 팀이 아니다"));
	}
	const AERPlayerState* PS = Target->GetPlayerState<AERPlayerState>();
	if (!PS || !PS->IsDowned() || PS->IsDead())
	{
		return Fail(TEXT("빈사가 아니다"));
	}
	const float Dist = FVector::Dist2D(Reviver->GetActorLocation(), Target->GetActorLocation());
	const float Max = UERCombatSettings::Get().ReviveRange * 100.f + SlackCm;
	if (Dist > Max)
	{
		return Fail(FString::Printf(TEXT("멀다 %.0fcm > %.0fcm"), Dist, Max));
	}
	return true;
}

void UERReviveAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bCompleted = false;
	bTagged = false;
	AActor* Me = GetAvatarActorFromActorInfo();
	const AActor* Target = TriggerEventData ? TriggerEventData->Target.Get() : nullptr;
	FString Why;
	// 거리 여유 50cm — PC 가 사거리 안에서 보냈어도 RPC 가 오는 사이 조금 움직일 수 있다
	if (!HasAuthority(&ActivationInfo) || !CanRevive(Me, Target, 50.f, &Why))
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[소생] %s → %s 거절 — %s"), *GetNameSafe(Me), *GetNameSafe(Target), Why.IsEmpty() ? TEXT("서버 아님") : *Why);
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	TargetPawn = const_cast<AActor*>(Target);
	TargetPS = Cast<APawn>(Target)->GetPlayerState<AERPlayerState>();

	// 멈춰서 바라본다 — 가던 길(클릭 이동)이 살아 있으면 바로 끊긴다
	if (APawn* MyPawn = Cast<APawn>(Me))
	{
		if (AController* C = MyPawn->GetController())
		{
			C->StopMovement();
		}
		const FVector To = (Target->GetActorLocation() - MyPawn->GetActorLocation()).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			MyPawn->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		}
	}
	if (AERPlayerState* MyPS = Cast<AERPlayerState>(GetOwningActorFromActorInfo()))
	{
		MyPS->ClientPlayVoice(ERTags::Pres_Voice_Save);   // 쓰러진 아군 소생 시작 (Argument 73 · 본인)
	}
	// 소생 중 — **복제 loose 태그** (ActivationOwnedTags 는 클라에 안 간다) → 모든 머신이 소생 애니(resurrect) · 스킬 막기 (채집 State.Gathering 과 같은 길)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		// ⚠ 복제용은 클라로만 간다 — 서버 자신(리슨 호스트 화면 · 서버 쪽 스킬 차단)은 일반 loose 도 같이 (채집과 같은 짝 · 2026-10-08 PIE 서버 창 소생 동작 없음)
		ASC->AddLooseGameplayTag(ERTags::State_Reviving);
		ASC->AddReplicatedLooseGameplayTag(ERTags::State_Reviving);
		bTagged = true;
	}

	const float Seconds = UERCombatSettings::Get().ReviveSeconds;
	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Seconds);
	Wait->OnFinish.AddDynamic(this, &UERReviveAbility::OnChannelDone);
	Wait->ReadyForActivation();
	// CC 를 맞으면 끊김 (사용자 2026-10-08) — 부모 State.CC 하나로 (기절 · 에어본 · 속박 · 침묵 · 무장 해제 · 둔화 · 실명 · 시야 차단)
	UAbilityTask_WaitGameplayTagAdded* CC = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, ERTags::State_CC);
	CC->Added.AddDynamic(this, &UERReviveAbility::OnInterrupted);
	CC->ReadyForActivation();
	// 대상이 빈사에서 벗어남 — 사망 · 다른 아군이 먼저 소생
	UAbilityTask_WaitGameplayTagRemoved* Up = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(this, ERTags::State_Downed, TargetPawn.Get());
	Up->Removed.AddDynamic(this, &UERReviveAbility::OnInterrupted);
	Up->ReadyForActivation();

	UE_LOG(LogEternalReturn, Log, TEXT("[소생] %s → %s 시작 (%.1f초 · 움직이거나 CC 면 끊김)"), *GetNameSafe(Me), *GetNameSafe(Target), Seconds);
}

void UERReviveAbility::OnChannelDone()
{
	FString Why;
	// 끝날 때 다시 — 빈사자는 1m/s 로 기어갈 수 있다 → 여유 1m
	if (TargetPS.IsValid() && CanRevive(GetAvatarActorFromActorInfo(), TargetPawn.Get(), 100.f, &Why))
	{
		bCompleted = true;
		TargetPS->Revive(Cast<AERPlayerState>(GetOwningActorFromActorInfo()));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[소생] %s → %s 실패 — %s"), *GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(TargetPawn.Get()), *Why);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UERReviveAbility::OnInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UERReviveAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsActive() && !bCompleted && TargetPawn.IsValid())
	{
		const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		UE_LOG(LogEternalReturn, Log, TEXT("[소생] %s → %s 끊김 — %s"), *GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(TargetPawn.Get()),
			ASC && ASC->HasMatchingGameplayTag(ERTags::State_CC) ? TEXT("CC") : TargetPS.IsValid() && !TargetPS->IsDowned() ? TEXT("대상이 빈사가 아니다") : TEXT("이동 · 취소"));
	}
	if (bTagged)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(ERTags::State_Reviving);
			ASC->RemoveReplicatedLooseGameplayTag(ERTags::State_Reviving);
		}
		bTagged = false;
	}
	TargetPS.Reset();
	TargetPawn.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
