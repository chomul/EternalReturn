// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_PassiveBoost.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GAS/ERPassiveAbility.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment_LostHPStats.h"

void UERSkillFragment_PassiveBoost::OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const
{
	if (!Ctx.bAuthority || !Ctx.ASC || !PassiveSkill)
	{
		return;
	}
	const UERSkillFragment_LostHPStats* Stats = PassiveSkill->FindFragment<UERSkillFragment_LostHPStats>();
	UERPassiveAbility* Passive = nullptr;
	for (const FGameplayAbilitySpec& S : Ctx.ASC->GetActivatableAbilities())
	{
		if (S.SourceObject.Get() == PassiveSkill)
		{
			Passive = Cast<UERPassiveAbility>(S.GetPrimaryInstance());
			break;
		}
	}
	if (!Stats || !Passive || !Passive->IsActive())
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s 패시브 강화 — %s 의 패시브 · '잃은 체력 비례 스탯' 조각을 못 찾음"), *GetNameSafe(Ctx.Skill), *GetNameSafe(PassiveSkill));
		return;
	}
	FERSkillContext PassiveCtx = Passive->MakeContext(PassiveSkill, 1.f);
	Stats->ApplyTimed(PassiveCtx, UERSkillData::LevelValue(Duration, Ctx.Level));
}
