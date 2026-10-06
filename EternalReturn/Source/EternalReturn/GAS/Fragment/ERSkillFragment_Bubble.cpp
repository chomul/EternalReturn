// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Bubble.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"
#include "TimerManager.h"
#include "Presentation/ERPresentationComponent.h"

void UERSkillFragment_Bubble::OnExecute(FERSkillContext& Ctx) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!Ctx.bAuthority || !A || !Ctx.ASC)
	{
		return;
	}
	// 피해 면역 (CC 는 받는다)
	FGameplayEffectSpec Immune(GetDefault<UERTimedTagEffect>(), Ctx.ASC->MakeEffectContext(), 1.f);
	Immune.DynamicGrantedTags.AddTag(ERTags::State_DamageImmune);
	Immune.DynamicGrantedTags.AddTag(ERTags::State_Bubble);
	Immune.DynamicGrantedTags.AddTag(ERTags::Mode_Bubble);   // 감싼 자세 = 모드 (진입 skill02_start · 유지 skill02_idle · 해제 skill02_end — Argument 69 B1) · GE 수명과 같다
	Immune.SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Duration);
	Ctx.ASC->ApplyGameplayEffectSpecToSelf(Immune);
	// 이속 (자기 버프 조각과 같은 SetByCaller)
	const float Speed = UERSkillData::LevelValue(MoveSpeedMagnitude, Ctx.Level);
	if (MoveSpeedEffect && Speed != 0.f)
	{
		const FGameplayEffectSpecHandle H = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), MoveSpeedEffect, Ctx.Level);
		if (FGameplayEffectSpec* S = H.Data.Get())
		{
			S->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Duration);
			S->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, Speed);
			S->DynamicGrantedTags.AddTag(ERTags::State_Bubble);
			A->ApplySpecToSelf(H);
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 감싸기 %.1f초 — 피해 면역 · 이속 ×%.2f → 끝나면 터짐"),
		*GetNameSafe(Ctx.Avatar), *GetNameSafe(Ctx.Skill), Duration, Speed);

	// 면역이 끝나는 순간 터짐 — 어빌리티는 그 전에 끝나 있다. ⚠ 어빌리티에 묶은 타이머는 EndAbility 가 지운다 (E40) — 묶지 않은 람다로
	TWeakObjectPtr<UERGameplayAbility> WeakA(A);
	const UERSkillData* Burst = BurstSkill;
	FTimerHandle Unused;
	A->GetWorld()->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakA, Burst]()
	{
		UERGameplayAbility* Self = WeakA.Get();
		if (!Self || !Burst)
		{
			return;
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 감싸기 터짐 → %s"), *GetNameSafe(Self->GetAvatarActorFromActorInfo()), *GetNameSafe(Burst));
		if (AActor* Me = Self->GetAvatarActorFromActorInfo())
		{
			UERPresentationComponent::SendSfxCue(Me, ERTags::Pres_Sfx_SkillBurst_W, Me->GetActorLocation());   // Skill02_End — 맞힌 사람 없어도
		}
		Self->ExecuteOther(Burst, 1.f, /*bWithExecuteHooks=*/false);
	}), Duration, false);
}
