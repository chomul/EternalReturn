// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment.h"

#include "AbilitySystemComponent.h"
#include "GAS/ERAttributeSet.h"

bool UERSkillFragment::IsOnAt(const FERSkillContext& Ctx) const
{
	if (Ctx.Level < MinLevel)
	{
		return false;
	}
	if (AttackRangeCond == EERAttackRangeCond::Any)
	{
		return true;
	}
	const float Range = Ctx.ASC ? Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetAttackRangeAttribute()) : 0.f;
	const bool bMelee = Range <= ERSkillFragmentCond::MeleeRangeMax;
	return AttackRangeCond == EERAttackRangeCond::Melee ? bMelee : !bMelee;
}
