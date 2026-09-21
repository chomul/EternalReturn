// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_NextAttackBuff.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/ERSkillStateEffects.h"

void UERSkillFragment_NextAttackBuff::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.ASC || !Ctx.bAuthority || !Ctx.Skill)
	{
		return;
	}
	// 이미 있으면 새로 건다 (같은 스킬이면 갱신, 다른 스킬이면 나중 것).
	FGameplayTagContainer Q; Q.AddTag(ERTags::State_NextAttackBuff);
	Ctx.ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Q));

	const bool bInfinite = Duration <= 0.f;
	const FGameplayEffectSpecHandle SpecHandle = A->MakeOutgoingGameplayEffectSpec(
		A->GetCurrentAbilitySpecHandle(), A->GetCurrentActorInfo(), A->GetCurrentActivationInfo(),
		bInfinite ? UERNextAttackBuffInfiniteEffect::StaticClass() : UERNextAttackBuffEffect::StaticClass(), Ctx.Level);
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->DynamicGrantedTags.AddTag(ERTags::State_NextAttackBuff);
		if (!bInfinite)
		{
			Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Duration);
		}
		// ⭐ 평타가 이걸 읽어 "무슨 스킬의 피해·효과를 얹을지" 를 안다. 레벨은 Spec Level 로 간다.
		Spec->GetContext().AddSourceObject(Ctx.Skill);
		A->ApplySpecToSelf(SpecHandle);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 다음 평타 강화 대기 (%s)"),
			*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill),
			bInfinite ? TEXT("만료 없음") : *FString::Printf(TEXT("%.1f초"), Duration));
	}
}
