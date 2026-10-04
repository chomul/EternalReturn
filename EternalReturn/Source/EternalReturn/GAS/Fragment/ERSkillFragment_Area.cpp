// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Area.h"

#include "Combat/ERSkillAreaActor.h"
#include "EternalReturn.h"
#include "AbilitySystemComponent.h"
#include "GAS/ERAttributeSet.h"
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
	const FVector Where = bFollowCaster ? Ctx.Avatar->GetActorLocation() : Ctx.AimPoint;

	// 펄스 수 — 시전 순간 확정 (추가 방어력 = 최종 − 기본 · ERDamageExecution 의 "추가" 와 같은 정의)
	int32 Pulses = 0;
	float Interval = TickInterval;
	if (BasePulses > 0)
	{
		float BonusDef = 0.f;
		if (Ctx.ASC)
		{
			BonusDef = FMath::Max(0.f, Ctx.ASC->GetNumericAttribute(UERAttributeSet::GetDefenseAttribute()) - Ctx.ASC->GetNumericAttributeBase(UERAttributeSet::GetDefenseAttribute()));
		}
		Pulses = BasePulses + (BonusDefensePerPulse > 0.f ? FMath::FloorToInt(BonusDef / BonusDefensePerPulse) : 0);
		Interval = Duration / Pulses;
		UE_LOG(LogEternalReturn, Log, TEXT("[장판] %s 펄스 %d회 (기본 %d + 추가 방어력 %.1f / %.0f) · %.2f초마다"),
			*GetNameSafe(Ctx.Skill), Pulses, BasePulses, BonusDef, BonusDefensePerPulse, Interval);
	}
	if (AERSkillAreaActor* Area = Ctx.Avatar->GetWorld()->SpawnActor<AERSkillAreaActor>(AERSkillAreaActor::StaticClass(), Where, FRotator::ZeroRotator, Params))
	{
		if (bFollowCaster)
		{
			Area->AttachToActor(Ctx.Avatar, FAttachmentTransformRules::KeepWorldTransform);
		}
		Area->InitializeFromFragment(Ctx.Ability, Ctx.Skill, Ctx.Level, Duration, Radius, Interval, DamageDecay, this, Pulses);
	}
}
