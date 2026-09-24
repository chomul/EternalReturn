// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERTargeting.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/ERTeamStatics.h"
#include "GAS/ERGameplayTags.h"
#include "ERCollisionChannels.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	/** ⭐ m -> uu 변환은 여기 한 곳에서만 한다. 판정 함수마다 곱하면 언젠가 하나를 빠뜨린다. */
	constexpr float MetersToUU = 100.f;

	FORCEINLINE float M(float Meters) { return Meters * MetersToUU; }
	FORCEINLINE float ToMeters(float UU) { return UU / MetersToUU; }

	/**
	 * ⚠ 이 파일의 **형상 판정**(반경 · 부채꼴 · 직선)은 대상을 **한 점(발밑)** 으로 본다. 캡슐 반경을 무시한다.
	 *   (SingleTarget 의 사거리만 2026-09-23 부터 대상 판정면 기준이다 — DistanceToSurface · Argument 32)
	 *
	 *   가까울수록 오차가 커진다. 캡슐 반경 42uu 기준으로 대상이 차지하는 각도는
	 *     1m -> 약 ±23도 / 3m -> 약 ±8도 / 5m -> 약 ±4.8도
	 *   즉 65도 부채꼴(±32.5도)에서 1m 거리 대상은 몸통이 절반 걸쳐 있어도
	 *   중심점이 밖이면 안 맞는다. 반경 판정도 같은 성격이다.
	 *
	 *   고치려면 허용 각도에 asin(대상반경/거리) 를 더하면 된다(몇 줄).
	 *   지금은 넣지 않는다 — 점 판정은 예측 가능하고 MOBA 에서 흔한 선택이며,
	 *   근접 체감이 실제로 문제인지는 스킬을 붙여봐야 안다.
	 *   ⚠ 원작이 어느 쪽인지는 확인하지 못했다 (미확인).
	 */

	/** bIgnoreZ 면 수평 거리, 아니면 3D 거리. 단위는 uu. */
	float MeasureDistance(const FVector& A, const FVector& B, bool bIgnoreZ)
	{
		if (bIgnoreZ)
		{
			return FVector::Dist2D(A, B);
		}
		return FVector::Dist(A, B);
	}

	/** 팀 필터 + 무시 목록 + 자기 자신 처리. 여기 통과한 것만 결과에 들어간다. */
	bool PassesFilter(const AActor* Candidate, const FTargetQuery& Q)
	{
		if (!Candidate)
		{
			return false;
		}

		if (Q.IgnoredActors.Contains(Candidate))
		{
			return false;
		}

		// ⭐ 판정 대상 자격 — **ASC 가 있어야** 한다 (2026-09-23).
		//   커서는 맵 소품 · 지형(StaticMeshActor, BlockAll 프로파일)도 짚는다. 그대로 두면 평타가 벽을 때리고
		//   쿨다운 · 이동 정지를 소비한다 (로그 06:55 `대상 StaticMeshActor_34`).
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate);
		if (!ASC)
		{
			return false;
		}
		// ⭐ 시체 · 무적 연출은 대상이 아니다. 야생동물 시체는 클릭(루팅)만 되고 맞지는 않는다 — SingleTarget 은
		//   오버랩이 아니라 "커서가 짚은 액터" 라서 콜리전을 꺼도 걸러지지 않는다. 그래서 태그로 막는다.
		if (ASC->HasMatchingGameplayTag(ERTags::State_Untargetable))
		{
			return false;
		}

		if (Candidate == Q.Instigator)
		{
			return Q.bIncludeInstigator;
		}

		switch (Q.TeamFilter)
		{
		case ETargetTeamFilter::All:
			return true;

		case ETargetTeamFilter::Ally:
			// ⭐ 무소속(야생동물)은 아군이 아니다. IsSameTeam 이 그 규칙을 갖고 있다.
			return ERTeamStatics::IsSameTeam(Q.Instigator, Candidate);

		case ETargetTeamFilter::Enemy:
			// ⭐ !IsSameTeam 으로 쓰지 않는다. 무소속끼리도 적이어야 하고(곰 vs 늑대),
			//    자기 자신은 적이 아니다. 그 규칙은 IsHostile 이 갖고 있다.
			return ERTeamStatics::IsHostile(Q.Instigator, Candidate);
		}

		return false;
	}

	/** 오버랩 결과에서 액터만 뽑아 필터를 통과시킨 뒤 중복을 없앤다. */
	void GatherActors(const TArray<FOverlapResult>& Overlaps, const FTargetQuery& Q, TArray<AActor*>& Out)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Actor = Overlap.GetActor();
			if (!PassesFilter(Actor, Q))
			{
				continue;
			}
			// 한 액터에 콜리전 컴포넌트가 여러 개면 같은 액터가 여러 번 나온다.
			Out.AddUnique(Actor);
		}
	}

	/** 가까운 순 정렬 + 거리 기록. 사거리 하한/상한도 여기서 최종 확인한다. */
	void SortAndMeasure(TArray<AActor*>& Actors, const FVector& Origin, const FTargetQuery& Q,
	                    float RangeMinUU, float RangeMaxUU, TArray<float>* OutDistancesM)
	{
		// 거리를 한 번만 재고 그 값으로 정렬·기록까지 끝낸다.
		TArray<TPair<float, AActor*>> Measured;
		Measured.Reserve(Actors.Num());

		for (AActor* Actor : Actors)
		{
			const float Dist = MeasureDistance(Origin, ERTargeting::GetTargetingLocation(Actor), Q.bIgnoreZ);
			if (Dist < RangeMinUU || (RangeMaxUU > 0.f && Dist > RangeMaxUU))
			{
				continue;
			}
			Measured.Emplace(Dist, Actor);
		}

		Measured.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B)
		{
			return A.Key < B.Key;
		});

		Actors.Reset();
		if (OutDistancesM)
		{
			OutDistancesM->Reset();
		}
		for (const TPair<float, AActor*>& Pair : Measured)
		{
			Actors.Add(Pair.Value);
			if (OutDistancesM)
			{
				OutDistancesM->Add(ToMeters(Pair.Key));
			}
		}
	}

	/** 구 오버랩 한 번. 반경은 uu. */
	void OverlapSphere(const UWorld* World, const FVector& Center, float RadiusUU,
	                   const FTargetQuery& Q, TArray<AActor*>& Out)
	{
		if (RadiusUU <= 0.f)
		{
			return;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(ERTargeting), /*bTraceComplex=*/false);
		if (Q.Instigator)
		{
			Params.AddIgnoredActor(Q.Instigator);
		}

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity,
			ERCollisionChannel::SkillTarget, FCollisionShape::MakeSphere(RadiusUU), Params);

		GatherActors(Overlaps, Q, Out);
	}
}

FVector ERTargeting::GetTargetingLocation(const AActor* Actor)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}

	// ACharacter 는 캡슐이 루트다 -> GetActorLocation() 은 캡슐 중심이다.
	// 발밑으로 내려서 키 차이의 영향을 없앤다.
	if (const ACharacter* Character = Cast<const ACharacter>(Actor))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			return Actor->GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());
		}
	}

	// 캡슐이 없는 액터(설치물·구조물)는 액터 위치를 그대로 쓴다.
	// 자체 결정값이다 - 원작 기준은 확인하지 못했다.
	return Actor->GetActorLocation();
}

float ERTargeting::DistanceToSurface(const AActor* Target, const FVector& From, bool bIgnoreZ)
{
	if (!Target)
	{
		return TNumericLimits<float>::Max();
	}
	const float CenterDist = MeasureDistance(From, GetTargetingLocation(Target), bIgnoreZ);

	// 판정을 받는 컴포넌트 = SkillTarget 에 응답하는 것. 야생동물은 HitBox, 실험체는 캡슐이다.
	const UPrimitiveComponent* Shape = FindSkillShape(Target);
	if (!Shape)
	{
		return CenterDist;   // 판정 컴포넌트가 없다 — 기존 기준 그대로
	}

	FVector OnSurface = FVector::ZeroVector;
	const float Dist3D = Shape->GetClosestPointOnCollision(From, OnSurface);
	if (Dist3D < 0.f)
	{
		return CenterDist;   // 심플 콜리전이 없다 (Dist3D == 0 이면 안에 들어와 있다 → 거리 0)
	}
	// 표면이 중심보다 멀 수는 없다. 이상한 형상에서 뒤집히지 않게 막는다.
	return FMath::Min(CenterDist, MeasureDistance(From, OnSurface, bIgnoreZ));
}

const UPrimitiveComponent* ERTargeting::FindSkillShape(const AActor* Actor)
{
	const UPrimitiveComponent* Shape = nullptr;
	if (Actor)
	{
		Actor->ForEachComponent<UPrimitiveComponent>(false, [&Shape](const UPrimitiveComponent* Prim)
		{
			// Overlap 도 판정 대상이다 — 실험체 캡슐은 Pawn 프로파일에서 Overlap, 야생동물 HitBox 도 Overlap 이다.
			if (!Shape && Prim->IsQueryCollisionEnabled() && Prim->GetCollisionResponseToChannel(ERCollisionChannel::SkillTarget) != ECR_Ignore)
			{
				Shape = Prim;
			}
		});
	}
	return Shape;
}

float ERTargeting::SingleTargetDistance(const AActor* Instigator, const FVector& Origin, const AActor* Target, bool bIgnoreZ)
{
	float Dist = DistanceToSurface(Target, Origin, bIgnoreZ);

	// ⭐ 시전자 쪽 (Argument 32 · F12-04): 몸이 캡슐보다 큰 시전자(야생동물)는 **자기 몸 끝에서** 잰다.
	//   늑대 사거리 1.4m 인데 주둥이가 중심에서 1.47m 앞이라, 중심 기준이면 제 코앞도 못 문다.
	//   몸 끝까지의 길이 = (중심 → 대상) − (시전자 표면 → 대상). 방향에 따라 다르다 (옆구리는 짧고 머리 쪽은 길다).
	const UPrimitiveComponent* Body = FindSkillShape(Instigator);
	if (Body && Instigator && Body != Instigator->GetRootComponent() && Target)
	{
		const FVector TargetPoint = GetTargetingLocation(Target);
		const float CenterToTarget = MeasureDistance(Origin, TargetPoint, bIgnoreZ);
		const float SurfaceToTarget = DistanceToSurface(Instigator, TargetPoint, bIgnoreZ);
		Dist = FMath::Max(0.f, Dist - FMath::Max(0.f, CenterToTarget - SurfaceToTarget));
	}
	return Dist;
}

float ERTargeting::GetDistanceAlpha(float Distance, float RangeMin, float RangeMax)
{
	const float Span = RangeMax - RangeMin;
	if (FMath::IsNearlyZero(Span))
	{
		// 하한과 상한이 같으면 보간할 구간이 없다. 0으로 나누지 않는다.
		return 0.f;
	}
	return FMath::Clamp((Distance - RangeMin) / Span, 0.f, 1.f);
}

FTargetResult ERTargeting::Query(const UWorld* World, const FTargetQuery& Q)
{
	switch (Q.Shape)
	{
	case ESkillTargeting::SingleTarget: return QuerySingleTarget(World, Q);
	case ESkillTargeting::SelfRadius:   return QuerySelfRadius(World, Q);
	case ESkillTargeting::Projectile:   return QueryProjectile(World, Q);
	case ESkillTargeting::GroundCircle: return QueryGroundCircle(World, Q);
	case ESkillTargeting::Cone:         return QueryCone(World, Q);
	case ESkillTargeting::DualRadius:   return QueryDualRadius(World, Q);
	}
	return FTargetResult();
}

FTargetResult ERTargeting::QuerySingleTarget(const UWorld* World, const FTargetQuery& Q)
{
	FTargetResult Result;
	if (!World || !Q.DesignatedTarget || !PassesFilter(Q.DesignatedTarget, Q))
	{
		return Result;
	}

	// ⭐ 대상 **판정면**까지 (Argument 32). 중심 기준이면 몸이 큰 야생동물은 붙어도 빗나간다. 시전자가 큰 몸이면 그 표면에서.
	const float Dist = SingleTargetDistance(Q.Instigator, Q.Origin, Q.DesignatedTarget, Q.bIgnoreZ);
	if (Dist < M(Q.RangeMin) || Dist > M(Q.RangeMax))
	{
		return Result;
	}

	Result.HitActors.Add(Q.DesignatedTarget);
	Result.Distances.Add(ToMeters(Dist));
	return Result;
}

FTargetResult ERTargeting::QuerySelfRadius(const UWorld* World, const FTargetQuery& Q)
{
	FTargetResult Result;
	if (!World)
	{
		return Result;
	}

	TArray<AActor*> Hits;
	OverlapSphere(World, Q.Origin, M(Q.RangeMax), Q, Hits);
	SortAndMeasure(Hits, Q.Origin, Q, M(Q.RangeMin), M(Q.RangeMax), &Result.Distances);

	Result.HitActors = MoveTemp(Hits);
	return Result;
}

FTargetResult ERTargeting::QueryGroundCircle(const UWorld* World, const FTargetQuery& Q)
{
	// 계산은 SelfRadius 와 같다. Origin 이 시전자가 아니라 지정 좌표라는 것만 다르고,
	// 그 차이는 호출자가 Origin 을 채우면서 이미 반영된다.
	return QuerySelfRadius(World, Q);
}

FTargetResult ERTargeting::QueryProjectile(const UWorld* World, const FTargetQuery& Q)
{
	FTargetResult Result;
	if (!World || Q.RangeMax <= 0.f)
	{
		return Result;
	}

	// 방향이 영벡터면 정규화가 0으로 나눈다.
	FVector Dir = Q.Direction;
	if (!Dir.Normalize())
	{
		return Result;
	}
	if (Q.bIgnoreZ)
	{
		Dir.Z = 0.f;
		if (!Dir.Normalize())
		{
			return Result;
		}
	}

	const float RangeUU  = M(Q.RangeMax);
	const float RadiusUU = FMath::Max(M(Q.ProjectileRadius), 1.f);
	const FVector End    = Q.Origin + Dir * RangeUU;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ERTargetingProjectile), /*bTraceComplex=*/false);
	if (Q.Instigator)
	{
		Params.AddIgnoredActor(Q.Instigator);
	}

	// 폭 0인 라인은 실전에서 잘 안 맞는다. 구를 쓸어서 두께를 준다.
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, Q.Origin, End, FQuat::Identity,
		ERCollisionChannel::SkillTarget, FCollisionShape::MakeSphere(RadiusUU), Params);

	TArray<AActor*> Actors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Actor = Hit.GetActor();
		if (PassesFilter(Actor, Q))
		{
			Actors.AddUnique(Actor);
		}
	}

	SortAndMeasure(Actors, Q.Origin, Q, M(Q.RangeMin), RangeUU, &Result.Distances);

	// 비관통이면 가장 가까운 하나만 남긴다. 정렬이 끝난 뒤라 앞에서 자르면 된다.
	if (!Q.bPenetrate && Actors.Num() > 1)
	{
		Actors.SetNum(1);
		Result.Distances.SetNum(1);
	}

	Result.HitActors = MoveTemp(Actors);
	return Result;
}

FTargetResult ERTargeting::QueryCone(const UWorld* World, const FTargetQuery& Q)
{
	FTargetResult Result;
	if (!World || Q.AngleDeg <= 0.f)
	{
		return Result;
	}

	FVector Dir = Q.Direction;
	if (!Dir.Normalize())
	{
		return Result;
	}
	if (Q.bIgnoreZ)
	{
		Dir.Z = 0.f;
		if (!Dir.Normalize())
		{
			return Result;
		}
	}

	// 먼저 원형으로 긁고 각도로 거른다.
	TArray<AActor*> Candidates;
	OverlapSphere(World, Q.Origin, M(Q.RangeMax), Q, Candidates);

	// ⭐ AngleDeg 는 전체 각도다. 다니엘 Q 65도 -> 중심에서 좌우 32.5도.
	//   ⚠ 대상 반경을 보정하지 않는다. 파일 상단 주석 참조.
	const float HalfAngleRad = FMath::DegreesToRadians(Q.AngleDeg * 0.5f);
	const float CosHalfAngle = FMath::Cos(HalfAngleRad);

	TArray<AActor*> InCone;
	for (AActor* Actor : Candidates)
	{
		FVector ToTarget = GetTargetingLocation(Actor) - Q.Origin;
		if (Q.bIgnoreZ)
		{
			ToTarget.Z = 0.f;
		}
		if (!ToTarget.Normalize())
		{
			// 시전자와 정확히 같은 위치. 각도를 정의할 수 없으므로 포함시킨다.
			InCone.Add(Actor);
			continue;
		}

		if (FVector::DotProduct(Dir, ToTarget) >= CosHalfAngle)
		{
			InCone.Add(Actor);
		}
	}

	SortAndMeasure(InCone, Q.Origin, Q, M(Q.RangeMin), M(Q.RangeMax), &Result.Distances);
	Result.HitActors = MoveTemp(InCone);
	return Result;
}

FTargetResult ERTargeting::QueryDualRadius(const UWorld* World, const FTargetQuery& Q)
{
	FTargetResult Result;
	if (!World)
	{
		return Result;
	}

	float InnerM = Q.RadiusInner;
	float OuterM = Q.RadiusOuter;

	// 뒤집혀 들어오면 조용히 잘못된 결과를 내지 않고 바로잡되 알린다.
	if (InnerM > OuterM)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ERTargeting] DualRadius: RadiusInner(%.2f) > RadiusOuter(%.2f). 값을 바꿔서 진행한다"),
			InnerM, OuterM);
		Swap(InnerM, OuterM);
	}

	TArray<AActor*> All;
	OverlapSphere(World, Q.Origin, M(OuterM), Q, All);

	const float InnerUU = M(InnerM);

	TArray<AActor*> Inner;
	TArray<AActor*> Outer;
	for (AActor* Actor : All)
	{
		const float Dist = MeasureDistance(Q.Origin, GetTargetingLocation(Actor), Q.bIgnoreZ);

		// ⭐ 경계는 중앙 우선(<=). 한 액터가 양쪽에 들어가면 피해가 두 번 들어간다.
		if (Dist <= InnerUU)
		{
			Inner.Add(Actor);
		}
		else
		{
			Outer.Add(Actor);
		}
	}

	SortAndMeasure(Inner, Q.Origin, Q, 0.f, InnerUU, nullptr);
	SortAndMeasure(Outer, Q.Origin, Q, M(Q.RangeMin), M(OuterM), &Result.Distances);

	Result.InnerHitActors = MoveTemp(Inner);
	Result.HitActors      = MoveTemp(Outer);
	return Result;
}
