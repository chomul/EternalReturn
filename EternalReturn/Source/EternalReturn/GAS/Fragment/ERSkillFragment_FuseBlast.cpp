// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_FuseBlast.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillPhaseEffect.h"

void UERSkillFragment_FuseBlast::OnExecute(FERSkillContext& Ctx) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.bAuthority)
	{
		return;
	}
	UERFuseBlastState* S = Ctx.GetState<UERFuseBlastState>(this);
	if (!S)
	{
		return;
	}
	S->Frag = this;
	Ctx.bSkipTargeting = true;   // 판정은 Delay 뒤
	Ctx.bKeepActive = true;      // 어빌리티를 끝내지 않는다 — OnFuseDone 이 끝낸다 (사망하면 취소로 같이 끝난다)

	// 도화선 동안 다른 빈사 스킬 막기 (UERDownedAbility 차단 칸) — 복제되는 GE 태그라 클라 선판정도 같은 답. 페이즈 GE 재사용
	if (Ctx.ASC)
	{
		const FGameplayEffectSpecHandle Spec = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(),
			A->GetCurrentActivationInfo(), UERSkillPhaseEffect::StaticClass(), Ctx.Level);
		if (Spec.Data.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_PhaseDuration, Delay);
			Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_DownedSkill_Fuse);
			S->FuseHandle = Ctx.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}

	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(A, Delay);
	Wait->OnFinish.AddDynamic(S, &UERFuseBlastState::OnFuseDone);
	Wait->ReadyForActivation();

	UERGameplayAbility::DebugDrawText(Ctx.Avatar, FString::Printf(TEXT("[빈사 스킬] 폭발 %.0f초 뒤"), Delay), FColor::Red);
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s <- %s 도화선 %.1f초 (취소 불가 · 그 사이 사망하면 폭발 없음)"),
		*GetNameSafe(Ctx.PlayerState), *GetNameSafe(Ctx.Skill), Delay);
}

void UERSkillFragment_FuseBlast::OnEnd(FERSkillContext& Ctx, bool bCancelled) const
{
	if (Ctx.bAuthority)
	{
		if (UERFuseBlastState* S = Ctx.GetState<UERFuseBlastState>(this))
		{
			S->ClearFuse();   // 폭발 · 사망 취소 어느 쪽이든
		}
	}
}

void UERFuseBlastState::ClearFuse()
{
	if (!FuseHandle.IsValid())
	{
		return;
	}
	const UERGameplayAbility* A = Cast<UERGameplayAbility>(GetOuter());
	const FGameplayAbilityActorInfo* Info = A ? A->GetCurrentActorInfo() : nullptr;
	if (UAbilitySystemComponent* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->RemoveActiveGameplayEffect(FuseHandle);
	}
	FuseHandle.Invalidate();
}

void UERFuseBlastState::OnFuseDone()
{
	UERGameplayAbility* A = Cast<UERGameplayAbility>(GetOuter());
	const UERSkillFragment_FuseBlast* F = Frag.Get();
	if (!A || !F || !A->IsActive())
	{
		return;
	}
	const UERSkillData* Skill = A->GetSkillData();
	AERPlayerState* PS = Cast<AERPlayerState>(A->GetOwningActorFromActorInfo());
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s <- %s 폭발"), *GetNameSafe(PS), *GetNameSafe(Skill));

	// 이 스킬의 모양 · 적중 조각 그대로 — 지금 내 자리 (모양 원점 = 시전자)
	A->ExecuteOther(Skill, 1.f, /*bWithExecuteHooks=*/false);

	// 끝내고 나서 죽인다 — 사망이 활성 어빌리티를 취소하므로 순서를 바꾸면 "취소" 로 끝난다
	const bool bKill = F->bKillSelf && PS && PS->IsDowned();
	A->EndFromFragment(/*bCancelled=*/false);
	if (bKill)
	{
		PS->SelfDestruct();
	}
	else if (F->bKillSelf)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s 폭발 때 빈사가 아니다 (그 사이 소생) — 사망 안 함 [자체]"), *GetNameSafe(PS));
	}
}
