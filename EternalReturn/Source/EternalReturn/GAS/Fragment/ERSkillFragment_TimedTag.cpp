// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_TimedTag.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"

void UERSkillFragment_TimedTag::OnExecute(FERSkillContext& Ctx) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.bAuthority)
	{
		return;
	}
	const float Seconds = UERSkillData::LevelValue(Duration, Ctx.Level);
	if (!Tag.IsValid() || Seconds <= 0.f)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 의 시간 태그 조각 — 태그 %s · 지속 %.1f (비었거나 0)"), *GetNameSafe(Ctx.Skill), *Tag.ToString(), Seconds);
		return;
	}
	const FGameplayEffectSpecHandle Spec = A->MakeOutgoingGameplayEffectSpec(A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(),
		A->GetCurrentActivationInfo(), UERTimedTagEffect::StaticClass(), Ctx.Level);
	if (!Spec.Data.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Seconds);
	Spec.Data->DynamicGrantedTags.AddTag(Tag);
	A->ApplySpecToSelf(Spec);
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 시간 태그 %s %.1f초"), *GetNameSafe(Ctx.Avatar), *GetNameSafe(Ctx.Skill), *Tag.ToString(), Seconds);
}
