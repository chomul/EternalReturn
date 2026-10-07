// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_CooldownOnHit.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERBleedEffect.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"

namespace
{
	void ReduceCooldown(FERSkillContext& Ctx, const FGameplayTagContainer& CooldownTags, float Sec);
}

void UERSkillFragment_CooldownOnHit::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (bFromHitEvent || !Ctx.bAuthority || Targets.IsEmpty() || !Ctx.ASC || !Ctx.Ability)
	{
		return;
	}
	ReduceCooldown(Ctx, CooldownTags.IsEmpty() ? Ctx.Ability->GetCooldownTagsForFragment() : CooldownTags, UERSkillData::LevelValue(Seconds, Ctx.Level));
}

void UERSkillFragment_CooldownOnHit::OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const
{
	if (!bFromHitEvent || !Ctx.bAuthority || !Ctx.ASC || CooldownTags.IsEmpty())
	{
		return;
	}
	if (bBasicAttackOnly && !HitTags.HasTag(ERTags::Damage_Type_BasicAttack))
	{
		return;
	}
	if (bRequireTargetMaxBleed && UERBleedEffect::GetStacks(Target, Ctx.ASC) < UERBleedEffect::MaxStacks)
	{
		return;
	}
	ReduceCooldown(Ctx, CooldownTags, UERSkillData::LevelValue(Seconds, Ctx.Level));
}

namespace
{
void ReduceCooldown(FERSkillContext& Ctx, const FGameplayTagContainer& CooldownTags, float Sec)
{
	if (Sec <= 0.f || CooldownTags.IsEmpty())
	{
		return;
	}
	// 시작 시각을 앞당기면 남은 시간이 준다 (복제 · 클라 쿨다운 표시도 엔진이)
	if (ERSkill::ShiftCooldown(Ctx.ASC, CooldownTags, Sec) > 0)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 적중 — %s 쿨다운 −%.1f초"),
			*GetNameSafe(Ctx.Avatar), *GetNameSafe(Ctx.Skill), *CooldownTags.ToStringSimple(), Sec);
	}
}
}   // namespace
