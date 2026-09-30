// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERSkillAreaActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/ERTargeting.h"
#include "DrawDebugHelpers.h"
#include "EternalReturn.h"
#include "HAL/IConsoleManager.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment_Area.h"
#include "TimerManager.h"
#include "Wildlife/ERWildlifeAIController.h"

AERSkillAreaActor::AERSkillAreaActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}


void AERSkillAreaActor::InitializeFromFragment(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, float InDuration, float InRadius, float InTickInterval, float InDecay,
	const UERSkillFragment_Area* InFragment)
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
	Fragment = InFragment;
	TickInterval = InTickInterval;
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

	// ER.Skill.DebugDraw 1 — 장판 반경을 다음 펄스까지 그린다 (서버 월드 · 리슨 서버 창). 맞으면 빨강
	static const IConsoleVariable* CVarDraw = IConsoleManager::Get().FindConsoleVariable(TEXT("ER.Skill.DebugDraw"));
	if (CVarDraw && CVarDraw->GetInt() != 0)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 20.f), Radius * 100.f, 32, Result.HitActors.IsEmpty() ? FColor::Purple : FColor::Red,
			false, TickInterval, 0, 2.f, FVector::RightVector, FVector::ForwardVector, false);
	}

	for (AActor* Target : Result.HitActors)
	{
		if (!Target) { continue; }
		int32& Count = HitCounts.FindOrAdd(Target);
		const float Scale = FMath::Pow(1.f - Decay, static_cast<float>(Count));
		++Count;
		A->ApplyOnTargets(Skill, { Target }, Scale, Level);
		UE_LOG(LogEternalReturn, Log, TEXT("[장판] %s -> %s %d번째 (피해 x%.2f)"), *GetNameSafe(Skill), *GetNameSafe(Target), Count, Scale);
	}

	// 밟으면 어그로 (F12.6-06 위클라인 유해 물질) — 시전자가 야생동물 · 보스일 때, 맞은 첫 사람의 팀으로
	AActor* Caster = A->GetAvatarActorFromActorInfo();
	if (Fragment && Fragment->bAggroOnHit && !Result.HitActors.IsEmpty())
	{
		if (const APawn* CasterPawn = Cast<APawn>(Caster))
		{
			if (AERWildlifeAIController* AI = Cast<AERWildlifeAIController>(CasterPawn->GetController()))
			{
				AI->NotifySteppedOnHazard(Result.HitActors[0]);
			}
		}
	}
	// 시전자가 장판 안이면 자기 효과 (F12.6-06 신경 가스)
	if (Fragment && !Fragment->CasterEffectsInside.IsEmpty() && Caster
		&& FVector::DistSquared2D(Caster->GetActorLocation(), GetActorLocation()) <= FMath::Square(Radius * 100.f))
	{
		ApplyCasterInside(Caster);
	}
}

void AERSkillAreaActor::ApplyCasterInside(AActor* Caster)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Caster);
	if (!ASC || !Fragment)
	{
		return;
	}
	for (const FERSelfEffect& SE : Fragment->CasterEffectsInside)
	{
		if (!SE.Effect)
		{
			continue;
		}
		FGameplayEffectContextHandle EffectCtx = ASC->MakeEffectContext();
		EffectCtx.AddSourceObject(Skill);
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SE.Effect, Level, EffectCtx);
		if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
		{
			Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, TickInterval * 1.5f);   // 다음 펄스 전에 안 끊기게 · 나가면 곧 풀림
			const float Mag = UERSkillData::LevelValueSigned(SE.Magnitude, Level);
			if (Mag != 0.f) { Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_OnHitMagnitude, Mag); }
			// 같은 GE 가 이미 있으면 새로 걸어 갱신 (쌓이지 않게 — GE 스택 정책 대신 먼저 뗀다)
			FGameplayEffectQuery Q;
			Q.EffectDefinition = SE.Effect;
			ASC->RemoveActiveEffects(Q);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec);
		}
	}
}
