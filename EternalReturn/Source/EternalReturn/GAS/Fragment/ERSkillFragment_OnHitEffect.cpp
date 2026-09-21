// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_OnHitEffect.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EternalReturn.h"
#include "GAS/ERCCLibrary.h"
#include "GAS/ERSkillData.h"

void UERSkillFragment_OnHitEffect::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (!Ctx.bAuthority || Targets.IsEmpty())
	{
		return;
	}
	if (!Effect)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s 의 적중 효과 조각에 GE 가 비어 있다."), *GetNameSafe(Ctx.Skill));
		return;
	}
	const float Dur = UERSkillData::LevelValue(Duration, Ctx.Level);
	const float Slow = UERSkillData::LevelValue(SlowPercent, Ctx.Level);
	const float Mag = UERSkillData::LevelValue(Magnitude, Ctx.Level);
	for (AActor* Target : Targets)
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
		{
			ERCC::ApplyCC(Ctx.ASC, TargetASC, Effect, Dur, Slow, Mag);
		}
	}
}
