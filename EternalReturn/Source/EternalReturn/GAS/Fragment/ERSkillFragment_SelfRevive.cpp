// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_SelfRevive.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillPhaseEffect.h"

void UERSkillFragment_SelfRevive::OnExecute(FERSkillContext& Ctx) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.bAuthority || !Ctx.ASC)
	{
		return;
	}
	UERSelfReviveState* S = Ctx.GetState<UERSelfReviveState>(this);
	if (!S)
	{
		return;
	}
	S->Frag = this;
	S->bSucceeded = false;
	Ctx.bSkipTargeting = true;
	Ctx.bKeepActive = true;   // 집중이 끝나거나 끊길 때까지 활성 — 끝내는 건 상태 객체 · 취소 (재입력 · CC · 사망)

	// 집중 태그 — 이동 막힘 (F06 State.Block.Movement 배선) · 다른 빈사 스킬 막힘 (UERDownedAbility 차단 칸). 페이즈 GE 재사용 (태그는 동적)
	const FGameplayEffectSpecHandle Spec = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(),
		A->GetCurrentActivationInfo(), UERSkillPhaseEffect::StaticClass(), Ctx.Level);
	if (Spec.Data.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_PhaseDuration, ChannelSeconds);
		Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_DownedSkill_SelfReviving);
		Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_Block_Movement);
		S->ChannelHandle = Ctx.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}

	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(A, ChannelSeconds);
	Wait->OnFinish.AddDynamic(S, &UERSelfReviveState::OnChannelDone);
	Wait->ReadyForActivation();

	// CC 가 오면 실패 (선딜 · 모드와 같은 축 — State.Block.Skill). 원작 (미확인)
	UAbilityTask_WaitGameplayTagAdded* WaitCC = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(A, ERTags::State_Block_Skill, nullptr, /*OnlyTriggerOnce=*/true);
	WaitCC->Added.AddDynamic(S, &UERSelfReviveState::OnInterruptedByCC);
	WaitCC->ReadyForActivation();

	// 아군이 먼저 일으켜 세우면 그냥 끝
	UAbilityTask_WaitGameplayTagRemoved* WaitUp = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(A, ERTags::State_Downed, nullptr, /*OnlyTriggerOnce=*/true);
	WaitUp->Removed.AddDynamic(S, &UERSelfReviveState::OnNoLongerDowned);
	WaitUp->ReadyForActivation();

	UERGameplayAbility::DebugDrawText(Ctx.Avatar, FString::Printf(TEXT("[빈사 스킬] 자가 소생 %.0f초"), ChannelSeconds), FColor::Green);
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s <- %s 자가 소생 집중 %.1f초 (재입력 · CC = 실패 → 쿨 %.0f%% 반환)"),
		*GetNameSafe(Ctx.PlayerState), *GetNameSafe(Ctx.Skill), ChannelSeconds, FailRefund * 100.f);
}

void UERSkillFragment_SelfRevive::OnEnd(FERSkillContext& Ctx, bool bCancelled) const
{
	if (!Ctx.bAuthority)
	{
		return;
	}
	UERSelfReviveState* S = Ctx.GetState<UERSelfReviveState>(this);
	if (!S || !S->ChannelHandle.IsValid())
	{
		return;   // 이미 정리됨 (EndAbility 훅이 두 번 불려도 한 번)
	}
	S->ClearChannel();
	if (!bCancelled || S->bSucceeded || !Ctx.ASC || !Ctx.Ability)
	{
		return;
	}
	// 실패 — 쿨다운의 FailRefund 만큼 남은 시간을 줄인다 (원작 "쿨다운의 80% 를 돌려받습니다" — 전체 쿨 기준)
	float Refunded = 0.f;
	const float Now = Ctx.ASC->GetWorld()->GetTimeSeconds();
	const FGameplayTagContainer& CooldownTags = Ctx.Ability->GetCooldownTagsForFragment();
	for (const FActiveGameplayEffectHandle& H : Ctx.ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags)))
	{
		if (const FActiveGameplayEffect* Active = Ctx.ASC->GetActiveGameplayEffect(H))
		{
			const float Shift = FMath::Min(Active->GetDuration() * FailRefund, Active->GetTimeRemaining(Now));
			if (Shift > 0.f)
			{
				Ctx.ASC->ModifyActiveEffectStartTime(H, -Shift);
				Refunded = FMath::Max(Refunded, Shift);
			}
		}
	}
	UERGameplayAbility::DebugDrawText(Ctx.Avatar, FString::Printf(TEXT("[빈사 스킬] 자가 소생 실패 · 쿨 %.1f초 반환"), Refunded), FColor::Orange);
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s <- %s 자가 소생 실패 — 쿨다운 %.1f초 반환"), *GetNameSafe(Ctx.PlayerState), *GetNameSafe(Ctx.Skill), Refunded);
}

void UERSelfReviveState::ClearChannel()
{
	if (!ChannelHandle.IsValid())
	{
		return;
	}
	const UERGameplayAbility* A = Cast<UERGameplayAbility>(GetOuter());
	const FGameplayAbilityActorInfo* Info = A ? A->GetCurrentActorInfo() : nullptr;
	if (UAbilitySystemComponent* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->RemoveActiveGameplayEffect(ChannelHandle);
	}
	ChannelHandle.Invalidate();
}

void UERSelfReviveState::OnChannelDone()
{
	UERGameplayAbility* A = Cast<UERGameplayAbility>(GetOuter());
	if (!A || !A->IsActive())
	{
		return;
	}
	bSucceeded = true;
	ClearChannel();
	if (AERPlayerState* PS = Cast<AERPlayerState>(A->GetOwningActorFromActorInfo()))
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s 자가 소생 성공"), *GetNameSafe(PS));
		PS->Revive(PS);
	}
	// Revive 가 State.Downed 를 지워 OnNoLongerDowned 가 먼저 끝냈을 수 있다
	if (A->IsActive())
	{
		A->EndFromFragment(/*bCancelled=*/false);
	}
}

void UERSelfReviveState::OnInterruptedByCC()
{
	UERGameplayAbility* A = Cast<UERGameplayAbility>(GetOuter());
	if (A && A->IsActive())
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s 자가 소생 — CC 로 끊김"), *GetNameSafe(A->GetOwningActorFromActorInfo()));
		A->EndFromFragment(/*bCancelled=*/true);
	}
}

void UERSelfReviveState::OnNoLongerDowned()
{
	UERGameplayAbility* A = Cast<UERGameplayAbility>(GetOuter());
	if (A && A->IsActive())
	{
		if (!bSucceeded)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s 자가 소생 중 빈사가 풀림 (아군 소생) — 끝 · 반환 없음"), *GetNameSafe(A->GetOwningActorFromActorInfo()));
			bSucceeded = true;   // 실패가 아니다 — 반환 안 함
		}
		A->EndFromFragment(/*bCancelled=*/false);
	}
}
