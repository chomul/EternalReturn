// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Area.h"

#include "Combat/ERSkillAreaActor.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"

void UERSkillFragment_Area::OnExecute(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || !Ctx.Avatar || !Ctx.Ability)
	{
		return;
	}
	Ctx.bSkipTargeting = true;   // 판정은 장판이 한다

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Ctx.Avatar;
	if (AERSkillAreaActor* Area = Ctx.Avatar->GetWorld()->SpawnActor<AERSkillAreaActor>(AERSkillAreaActor::StaticClass(), Ctx.AimPoint, FRotator::ZeroRotator, Params))
	{
		Area->InitializeFromFragment(Ctx.Ability, Ctx.Skill, Ctx.Level, Duration, Radius, TickInterval, DamageDecay, this);
	}
}
