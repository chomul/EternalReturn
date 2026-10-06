// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Support.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERLifestealEffect.h"
#include "GAS/ERShield.h"
#include "GAS/ERSkillData.h"
#include "Growth/ERGrowthComponent.h"
#include "Presentation/ERPresentationComponent.h"

float FERSupportAmount::Evaluate(const FERSkillContext& Ctx) const
{
	const UERGrowthComponent* Growth = Ctx.PlayerState ? Ctx.PlayerState->GetGrowth() : nullptr;
	const int32 CharLevel = Growth ? Growth->GetLevel() : 0;
	const float SkillAmp = Ctx.ASC ? Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetSkillAmpAttribute()) : 0.f;
	return UERSkillData::LevelValue(Base, Ctx.Level)
		+ CharLevel * UERSkillData::LevelValue(PerCharLevel, Ctx.Level)
		+ SkillAmp * UERSkillData::LevelValue(SkillAmpRatio, Ctx.Level);
}

void UERSkillFragment_Heal::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (!Ctx.bAuthority || !Ctx.ASC)
	{
		return;
	}
	const float Heal = Amount.Evaluate(Ctx);
	if (Heal <= 0.f)
	{
		return;
	}
	for (AActor* T : Targets)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(T);
		if (!TargetASC)
		{
			continue;
		}
		FGameplayEffectSpec Spec(GetDefault<UERLifestealEffect>(), Ctx.ASC->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(ERTags::SetByCaller_HealAmount, Heal);
		Ctx.ASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
		if (HitSfx.IsValid() && T)
		{
			UERPresentationComponent::SendSfxCue(Ctx.Avatar, HitSfx, T->GetActorLocation());
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 회복 %.1f × %d명"), *GetNameSafe(Ctx.Skill), Heal, Targets.Num());
}

void UERSkillFragment_Shield::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (!Ctx.bAuthority)
	{
		return;
	}
	const float Value = Amount.Evaluate(Ctx);
	for (AActor* T : Targets)
	{
		ERShield::Apply(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(T), Value, Duration, GetNameSafe(Ctx.Skill));
	}
}
