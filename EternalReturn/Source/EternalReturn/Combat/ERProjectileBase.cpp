// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERProjectileBase.h"

#include "Combat/ERTargeting.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

AERProjectileBase::AERProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);   // ⭐ 위치는 안 보낸다 — 0.4초짜리를 위치 복제로 받으면 몇 번 못 받는다. 발사 값으로 클라가 그린다

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(25.f);
	// ⭐ 벽을 **통과**한다 — 원작 규칙 (사용자 2026-10-01 "원래도 벽은 통과함"). 이동 충돌 없음 ·
	//   사람 판정은 SkillTarget 채널 스윕이 따로 한다 (다른 판정 모양과 같은 필터). 벽에 막히는 투사체가 생기면 파생에서 켠다
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetGenerateOverlapEvents(false);
	RootComponent = Collision;

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bAutoActivate = false;   // 발사 값이 오면 StartFlight 가 켠다
}

void AERProjectileBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AERProjectileBase, Launch, COND_InitialOnly);
}

void AERProjectileBase::InitLaunch(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, const FTargetQuery& InFilter, const FERProjectileLaunch& InLaunch, int32 InShotIndex, bool bInPierce)
{
	ShotIndex = InShotIndex;
	bPierce = bInPierce;
	Ability = InAbility;
	Skill = InSkill;
	Level = InLevel;
	Filter = InFilter;
	Launch = InLaunch;
}

void AERProjectileBase::BeginPlay()
{
	Super::BeginPlay();
	// 서버: InitLaunch 가 먼저 채웠다. 클라: 첫 복제 값이 BeginPlay 전에 들어와 있다 (없으면 OnRep 이 시작한다)
	if (Launch.SpeedUU > 0.f)
	{
		StartFlight();
	}
}

void AERProjectileBase::OnRep_Launch()
{
	if (HasActorBegunPlay() && Launch.SpeedUU > 0.f)
	{
		StartFlight();
	}
}

void AERProjectileBase::StartFlight()
{
	if (bFlying || bEnded)
	{
		return;
	}
	bFlying = true;
	SetActorLocation(Launch.Start);
	LastLocation = Launch.Start;
	Collision->SetSphereRadius(Launch.RadiusUU);
	Movement->OnProjectileStop.AddDynamic(this, &AERProjectileBase::OnMovementStop);
	Movement->InitialSpeed = Launch.SpeedUU;
	Movement->MaxSpeed = Launch.SpeedUU;
	Movement->Velocity = FVector(Launch.Direction) * Launch.SpeedUU;
	Movement->Activate(true);
	if (HasAuthority())
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[투사체] %s 발사 — %s · %.0f m/s · 사거리 %.1fm · 반경 %.2fm · 높이 %.0f"),
			*GetNameSafe(this), *GetNameSafe(Skill), Launch.SpeedUU / 100.f, Launch.RangeUU / 100.f, Launch.RadiusUU / 100.f, Launch.Start.Z);
	}
}

void AERProjectileBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFlying || bEnded)
	{
		return;
	}
	++FlightTicks;
	const FVector Now = GetActorLocation();
	// ER.Skill.DebugDraw 1 — 지나온 길(초록) · 판정 굵기 (서버 월드 = 리슨 서버 창)
	static const IConsoleVariable* CVarDraw = IConsoleManager::Get().FindConsoleVariable(TEXT("ER.Skill.DebugDraw"));
	const bool bDraw = HasAuthority() && CVarDraw && CVarDraw->GetInt() != 0;
	if (bDraw)
	{
		DrawDebugLine(GetWorld(), LastLocation, Now, FColor::Green, false, 1.5f, 0, 3.f);
		DrawDebugSphere(GetWorld(), Now, Launch.RadiusUU, 8, FColor(0, 255, 0, 80), false, 0.f, 0, 1.f);
	}
	if (HasAuthority())
	{
		// 지나온 구간을 쓸어 판정 — 가까운 순. 하나 맞고 끝나면 뒤는 안 본다 (비관통)
		for (AActor* Target : ERTargeting::SweepSegment(GetWorld(), LastLocation, Now, Launch.RadiusUU, Filter))
		{
			if (CanHit(Target))
			{
				OnHitTarget(Target);
				if (bEnded)
				{
					return;
				}
			}
		}
	}
	TraveledUU += FVector::Dist(LastLocation, Now);
	LastLocation = Now;
	if (Launch.RangeUU > 0.f && TraveledUU >= Launch.RangeUU)
	{
		EndFlight(TEXT("사거리 끝"));
	}
}

bool AERProjectileBase::CanHit(const AActor* Target) const
{
	if (Launch.bOnlyHomingTarget && Target != Launch.HomingTarget)
	{
		return false;   // 대상만 (원거리 평타 · Argument 60 H1)
	}
	return Target && !Filter.IgnoredActors.Contains(Target);
}

void AERProjectileBase::OnHitTarget(AActor* Target)
{
	Filter.IgnoredActors.Add(Target);   // 같은 대상은 한 번 (관통이어도)
	static const IConsoleVariable* CVarDraw = IConsoleManager::Get().FindConsoleVariable(TEXT("ER.Skill.DebugDraw"));
	if (CVarDraw && CVarDraw->GetInt() != 0)
	{
		DrawDebugSphere(GetWorld(), Target->GetActorLocation(), 45.f, 12, FColor::Red, false, 1.5f, 0, 2.f);   // 맞은 대상 (다른 판정 모양과 같은 표시)
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[투사체] %s 적중 %s — 날아간 거리 %.1fm / %.1fm · 비행 %.3f초 · 틱 %d · 대상까지 %.1fm"),
		*GetNameSafe(this), *GetNameSafe(Target), TraveledUU / 100.f, Launch.RangeUU / 100.f, GetGameTimeSinceCreation(), FlightTicks,
		FVector::Dist(GetActorLocation(), Target->GetActorLocation()) / 100.f);
	if (UERGameplayAbility* A = Ability.Get())
	{
		const float Ratio = Launch.RangeUU > 0.f ? FMath::Clamp(TraveledUU / Launch.RangeUU, 0.f, 1.f) : -1.f;
		A->ApplyProjectileHit(Skill, Target, Level, Ratio, ShotIndex);
	}
	if (!bPierce)
	{
		EndFlight(TEXT("적중"));
	}
}

void AERProjectileBase::OnMovementStop(const FHitResult& ImpactResult)
{
	EndFlight(TEXT("벽"));
}

void AERProjectileBase::EndFlight(const TCHAR* Why)
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;
	Movement->StopMovementImmediately();
	SetActorHiddenInGame(true);
	if (HasAuthority())
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[투사체] %s 끝 — %s (%.1fm)"), *GetNameSafe(this), Why, TraveledUU / 100.f);
		static const IConsoleVariable* CVarDraw = IConsoleManager::Get().FindConsoleVariable(TEXT("ER.Skill.DebugDraw"));
		if (CVarDraw && CVarDraw->GetInt() != 0)
		{
			DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 80.f), FString::Printf(TEXT("%s %.1fm"), Why, TraveledUU / 100.f), nullptr, FColor::Yellow, 1.5f, true);
		}
		SetLifeSpan(0.2f);   // 바로 지우면 같은 틱에 온 클라 복제가 끊긴다 — 잠깐 숨긴 채 두고 지운다
	}
}
