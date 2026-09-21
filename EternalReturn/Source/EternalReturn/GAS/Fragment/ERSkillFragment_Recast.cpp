// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Recast.h"

#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"

void UERSkillFragment_Recast::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.bAuthority || Ctx.bActivatedByRecast)
	{
		return;
	}
	const FGameplayTag RecastTag = A->GetRecastTag();
	if (!RecastTag.IsValid())
	{
		return;
	}
	if (bOnHitOnly && Targets.IsEmpty())
	{
		return;
	}
	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(
		A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(), UERRecastWindowEffect::StaticClass(), Ctx.Level);
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->DynamicGrantedTags.AddTag(RecastTag);
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Window);
		A->ApplySpecToSelf(SpecHandle);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 리캐스트 윈도우 %.1f초 (%s)"),
			*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), Window, *RecastTag.ToString());
	}
}
