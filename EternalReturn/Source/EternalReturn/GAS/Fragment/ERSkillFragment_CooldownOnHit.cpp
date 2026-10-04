// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_CooldownOnHit.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"

void UERSkillFragment_CooldownOnHit::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (!Ctx.bAuthority || Targets.IsEmpty() || !Ctx.ASC || !Ctx.Ability)
	{
		return;
	}
	const float Sec = UERSkillData::LevelValue(Seconds, Ctx.Level);
	const FGameplayTagContainer& CooldownTags = Ctx.Ability->GetCooldownTagsForFragment();
	if (Sec <= 0.f || CooldownTags.IsEmpty())
	{
		return;
	}
	// 시작 시각을 앞당기면 남은 시간이 준다 (복제 · 클라 쿨다운 표시도 엔진이)
	for (const FActiveGameplayEffectHandle& H : Ctx.ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags)))
	{
		Ctx.ASC->ModifyActiveEffectStartTime(H, -Sec);
		const FActiveGameplayEffect* E = Ctx.ASC->GetActiveGameplayEffect(H);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 적중 — 쿨다운 −%.1f초 (남은 %.1f초)"),
			*GetNameSafe(Ctx.Ability->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), Sec,
			E ? E->GetTimeRemaining(Ctx.ASC->GetWorld()->GetTimeSeconds()) : -1.f);
	}
}
