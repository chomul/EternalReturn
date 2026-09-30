// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_SelfBuff.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "TimerManager.h"

void UERSkillFragment_SelfBuff::OnExecute(FERSkillContext& Ctx) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.ASC || !Ctx.bAuthority || Effects.IsEmpty())
	{
		return;
	}
	const int32 Level = Ctx.Level;
	for (const FERSelfEffect& SE : Effects)
	{
		if (!SE.Effect)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 의 자기 버프 조각에 GE 가 비어 있다."), *GetNameSafe(Ctx.Skill));
			continue;
		}
		const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), SE.Effect, Level);
		FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
		if (!Spec)
		{
			continue;
		}
		const float Duration = UERSkillData::LevelValue(SE.Duration, Level);
		const float Magnitude = UERSkillData::LevelValueSigned(SE.Magnitude, Level);   // 음수 허용
		if (Duration > 0.f)   { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Duration); }
		if (Magnitude != 0.f) { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, Magnitude); }
		if (SE.Charges > 0)
		{
			// "다음 N회 기본 공격" — 평타가 적중마다 1 줄인다 (UERBasicAttackAbility).
			Spec->DynamicGrantedTags.AddTag(ERTags::State_ConsumeOnAttack);
			Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_Charges, static_cast<float>(SE.Charges));
		}
		const float Delay = UERSkillData::LevelValue(SE.StartDelay, Level);
		if (Delay > 0.f)
		{
			// "이동이 끝난 후" — 스펙은 지금 만들고 적용만 늦춘다. 람다 타이머 — EndAbility 가 못 지운다 (E19).
			TWeakObjectPtr<UAbilitySystemComponent> WeakASC(Ctx.ASC);
			FTimerHandle Unused;
			Ctx.ASC->GetWorld()->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakASC, SpecHandle]()
			{
				if (UAbilitySystemComponent* ASC = WeakASC.Get(); ASC && SpecHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
				}
			}), Delay, false);
		}
		else
		{
			A->ApplySpecToSelf(SpecHandle);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 자기 버프 %s (지속 %.1f · 크기 %.2f%s%s)"),
			*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), *SE.Effect->GetName(), Duration, Magnitude,
			SE.Charges > 0 ? *FString::Printf(TEXT(" · %d회"), SE.Charges) : TEXT(""),
			Delay > 0.f ? *FString::Printf(TEXT(" · %.1f초 뒤"), Delay) : TEXT(""));
	}
}
