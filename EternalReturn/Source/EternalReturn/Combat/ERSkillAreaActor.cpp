// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERSkillAreaActor.h"

#include "Combat/ERTargeting.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"
#include "TimerManager.h"

AERSkillAreaActor::AERSkillAreaActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}


void AERSkillAreaActor::InitializeFromFragment(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, float InDuration, float InRadius, float InTickInterval, float InDecay)
{
	if (!HasAuthority() || !InAbility || !InSkill)
	{
		Destroy();
		return;
	}
	Ability = InAbility;
	Skill = InSkill;
	Level = InLevel;
	Radius = InRadius;
	Decay = InDecay;
	SetLifeSpan(InDuration);
	UE_LOG(LogEternalReturn, Log, TEXT("[장판] %s 스폰 @%s — 반경 %.1fm · %.1f초 · %.1f초마다 · 감쇠 %.0f%%"),
		*GetNameSafe(InSkill), *GetActorLocation().ToCompactString(), InRadius, InDuration, InTickInterval, InDecay * 100.f);
	Pulse();
	GetWorldTimerManager().SetTimer(PulseTimer, this, &AERSkillAreaActor::Pulse, InTickInterval, true);
}

void AERSkillAreaActor::Pulse()
{
	UERGameplayAbility* A = Ability.Get();
	if (!A || !Skill || !A->GetAvatarActorFromActorInfo())
	{
		Destroy();
		return;
	}

	FTargetQuery Q;
	Q.Shape = ESkillTargeting::SelfRadius;
	Q.TeamFilter = Skill->Shape.TeamFilter;
	Q.RangeMax = Radius;
	Q.Instigator = A->GetAvatarActorFromActorInfo();
	Q.Origin = GetActorLocation();
	const FTargetResult Result = ERTargeting::Query(GetWorld(), Q);

	for (AActor* Target : Result.HitActors)
	{
		if (!Target) { continue; }
		int32& Count = HitCounts.FindOrAdd(Target);
		const float Scale = FMath::Pow(1.f - Decay, static_cast<float>(Count));
		++Count;
		A->ApplyOnTargets(Skill, { Target }, Scale, Level);
		UE_LOG(LogEternalReturn, Log, TEXT("[장판] %s -> %s %d번째 (피해 x%.2f)"), *GetNameSafe(Skill), *GetNameSafe(Target), Count, Scale);
	}
}
