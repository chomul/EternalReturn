// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_FailPenalty.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"

void UERSkillFragment_FailPenalty::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	// 평타 강화 · 2차 판정으로 재진입한 실행은 보지 않는다 — 이 스킬 자체의 판정만
	if (bOnMiss && Targets.IsEmpty() && !Ctx.bEnhancement)
	{
		Apply(Ctx, TEXT("빗나감"));
	}
}

void UERSkillFragment_FailPenalty::OnEnd(FERSkillContext& Ctx, bool bCancelled) const
{
	// ConsumeCastInterruptedByCC 는 한 번만 true — EndAbility 훅이 두 번 불려도 한 번 건다
	if (bOnCastInterrupted && bCancelled && Ctx.Ability && Ctx.Ability->ConsumeCastInterruptedByCC())
	{
		Apply(Ctx, TEXT("시전 중 CC"));
	}
}

void UERSkillFragment_FailPenalty::Apply(FERSkillContext& Ctx, const TCHAR* Why) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.ASC || !Ctx.bAuthority)
	{
		return;
	}
	for (const FERSelfEffect& SE : Effects)
	{
		if (!SE.Effect)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 의 실패 벌칙 조각에 GE 가 비어 있다."), *GetNameSafe(Ctx.Skill));
			continue;
		}
		// ⚠ 어빌리티가 이미 끝나는 중(OnEnd)일 수 있어 어빌리티 스펙 대신 ASC 에서 직접 만든다
		FGameplayEffectContextHandle EffectCtx = Ctx.ASC->MakeEffectContext();
		EffectCtx.AddSourceObject(Ctx.Skill);
		const FGameplayEffectSpecHandle SpecHandle = Ctx.ASC->MakeOutgoingSpec(SE.Effect, Ctx.Level, EffectCtx);
		FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
		if (!Spec)
		{
			continue;
		}
		const float Duration = UERSkillData::LevelValue(SE.Duration, Ctx.Level);
		const float Magnitude = UERSkillData::LevelValueSigned(SE.Magnitude, Ctx.Level);   // 음수 허용
		if (Duration > 0.f)   { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Duration); }
		if (Magnitude != 0.f) { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, Magnitude); }
		Ctx.ASC->ApplyGameplayEffectSpecToSelf(*Spec);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 실패 벌칙 (%s) %s (지속 %.1f · 크기 %.2f)"),
			*GetNameSafe(Ctx.Avatar), *GetNameSafe(Ctx.Skill), Why, *SE.Effect->GetName(), Duration, Magnitude);
	}
}
