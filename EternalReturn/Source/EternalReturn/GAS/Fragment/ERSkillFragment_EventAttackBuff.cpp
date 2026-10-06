// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_EventAttackBuff.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/Fragment/ERSkillFragment_NextAttackBuff.h"

void UERSkillFragment_EventAttackBuff::OnPassiveEvent(FERSkillContext& Ctx, const FGameplayEventData& Payload) const
{
	if (!Ctx.bAuthority || !Ctx.ASC || !Ctx.Ability)
	{
		return;
	}
	const FGameplayTagContainer& Cooldown = Ctx.Ability->GetCooldownTagsForFragment();
	if (!Cooldown.IsEmpty() && Ctx.ASC->HasAnyMatchingGameplayTags(Cooldown))
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %s — 쿨 중이라 장전 안 함"), *GetNameSafe(Ctx.Avatar), *Payload.EventTag.ToString());
		return;
	}
	if (Ctx.ASC->HasMatchingGameplayTag(ERTags::State_NextAttackBuff))
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s %s — 이미 강화 대기 중"), *GetNameSafe(Ctx.Avatar), *Payload.EventTag.ToString());
		return;
	}
	UERSkillFragment_NextAttackBuff::Grant(Ctx, Ctx.Skill, Ctx.Level, /*만료 없음*/ 0.f);
}
