// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Shape/ERSkillShapes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/ERTargeting.h"
#include "DrawDebugHelpers.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERSkillData.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	FVector Up(const FVector& V) { return V + FVector(0.f, 0.f, 20.f); }
	void Circle(const UWorld* W, const FVector& C, float RadiusUU, FColor Color, float Life, float Thick = 2.f)
	{
		DrawDebugCircle(W, Up(C), RadiusUU, 32, Color, false, Life, 0, Thick, FVector::RightVector, FVector::ForwardVector, false);
	}
}

// ── 대상 하나 ───────────────────────────────────────────────
void UERShape_Single::FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	Q.Shape = ESkillTargeting::SingleTarget;
	Q.RangeMax = Ctx.RangeOverride > 0.f ? Ctx.RangeOverride : Range;   // 평타 = 무기 사거리 (어빌리티가 넘긴다)

	// ⭐ 조준 보조 — 커서 아래 액터가 유효한 대상이 아니면(바닥 · 자기 · 아군) 조준점 반경 안 가장 가까운 대상 (자체 결정값 · 판정이 아니라 대상 고르기만)
	const float Assist = Ctx.Targets ? Ctx.Targets->AimAssistRadius : 0.f;
	const UWorld* World = Ctx.Avatar ? Ctx.Avatar->GetWorld() : nullptr;
	if (Assist <= 0.f || !World)
	{
		return;
	}
	if (Q.DesignatedTarget && !ERTargeting::Query(World, Q).IsEmpty())
	{
		return;   // 찍은 대상이 유효
	}
	FTargetQuery Near = Q;
	Near.Shape = ESkillTargeting::SelfRadius;   // 조준점 중심 원 — 가까운 순으로 온다 (F04)
	Near.Origin = Ctx.AimPoint;
	Near.RangeMin = 0.f;
	Near.RangeMax = Assist;
	Near.DesignatedTarget = nullptr;
	const FTargetResult Found = ERTargeting::Query(World, Near);
	if (!Found.HitActors.IsEmpty())
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] 조준 보조: %s -> %s"), *GetNameSafe(Q.DesignatedTarget), *GetNameSafe(Found.HitActors[0]));
		Q.DesignatedTarget = Found.HitActors[0];
	}
}

void UERShape_Single::Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const
{
	if (Q.DesignatedTarget)
	{
		DrawDebugLine(World, Up(Q.Origin), Q.DesignatedTarget->GetActorLocation(), Color, false, Life, 0, 2.f);
	}
	Circle(World, Q.Origin, Q.RangeMax * 100.f, FColor(Color.R, Color.G, Color.B, 80), Life, 1.f);
}

FString UERShape_Single::Describe() const
{
	return FString::Printf(TEXT("Single(사거리 %.1f · 최소 %.1f)"), Range, MinReach);
}

// ── 원 ─────────────────────────────────────────────────────
void UERShape_Circle::FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	if (ShapeOrigin == EERShapeOrigin::AimPoint)
	{
		Q.Shape = ESkillTargeting::GroundCircle;   // 반경 = RadiusOuter (E35)
		Q.RadiusOuter = Radius;
		Q.RangeMax = AimRange;
	}
	else
	{
		Q.Shape = ESkillTargeting::SelfRadius;
		Q.RangeMax = Radius;
	}
}

void UERShape_Circle::Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const
{
	Circle(World, Q.Origin, (Q.Shape == ESkillTargeting::GroundCircle ? Q.RadiusOuter : Q.RangeMax) * 100.f, Color, Life);
}

FString UERShape_Circle::Describe() const
{
	return FString::Printf(TEXT("Circle(%s · 반경 %.1f%s)"), ShapeOrigin == EERShapeOrigin::AimPoint ? *FString::Printf(TEXT("조준점 %.1f"), AimRange) : TEXT("시전자"),
		Radius, ForwardOffset > 0.f ? *FString::Printf(TEXT(" · 앞 %.1f"), ForwardOffset) : TEXT(""));
}

// ── 이중 원 ────────────────────────────────────────────────
void UERShape_DualCircle::FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	Q.Shape = ESkillTargeting::DualRadius;
	Q.RangeMax = OuterRadius;   // 아래층 DualRadius 는 RangeMax 를 바깥 반경으로 쓴다
	Q.RadiusInner = InnerRadius;
	Q.RadiusOuter = OuterRadius;
}

void UERShape_DualCircle::Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const
{
	Circle(World, Q.Origin, OuterRadius * 100.f, Color, Life);
	Circle(World, Q.Origin, InnerRadius * 100.f, FColor::Yellow, Life, 1.f);
}

FString UERShape_DualCircle::Describe() const
{
	return FString::Printf(TEXT("DualCircle(안 %.2f · 밖 %.2f%s)"), InnerRadius, OuterRadius, ForwardOffset > 0.f ? *FString::Printf(TEXT(" · 앞 %.1f"), ForwardOffset) : TEXT(""));
}

#if WITH_EDITOR
void UERShape_DualCircle::ValidateShape(FDataValidationContext& Context, const FString& Owner) const
{
	Super::ValidateShape(Context, Owner);
	if (InnerRadius >= OuterRadius)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s: 이중 원 안 %.2f >= 밖 %.2f"), *Owner, InnerRadius, OuterRadius)));
	}
}
#endif

// ── 부채꼴 ─────────────────────────────────────────────────
void UERShape_Cone::FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	Q.Shape = ESkillTargeting::Cone;
	Q.RangeMax = Length;
	Q.AngleDeg = AngleDeg;
}

void UERShape_Cone::Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const
{
	const FVector O = Up(Q.Origin);
	const float L = Q.RangeMax * 100.f;
	const FVector F = Q.Direction.GetSafeNormal2D();
	DrawDebugLine(World, O, O + F.RotateAngleAxis(-AngleDeg * 0.5f, FVector::UpVector) * L, Color, false, Life, 0, 2.f);
	DrawDebugLine(World, O, O + F.RotateAngleAxis(+AngleDeg * 0.5f, FVector::UpVector) * L, Color, false, Life, 0, 2.f);
	Circle(World, Q.Origin, L, FColor(Color.R, Color.G, Color.B, 80), Life, 1.f);
}

FString UERShape_Cone::Describe() const
{
	return FString::Printf(TEXT("Cone(길이 %.1f · 각 %.0f)"), Length, AngleDeg);
}

// ── 직선 ───────────────────────────────────────────────────
void UERShape_Line::FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	Q.Shape = ESkillTargeting::Projectile;   // 아래층 이름 — 직선 스윕
	Q.RangeMax = Length;
	Q.ProjectileRadius = Width * 0.5f;
	Q.bPenetrate = true;   // "첫 하나만" 은 대상 MaxTargets 1 이 자른다 (가까운 순)
}

void UERShape_Line::Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const
{
	const FVector O = Up(Q.Origin);
	const FVector End = O + Q.Direction.GetSafeNormal2D() * Q.RangeMax * 100.f;
	DrawDebugLine(World, O, End, Color, false, Life, 0, 3.f);
	DrawDebugCircle(World, End, Width * 50.f, 16, Color, false, Life, 0, 1.f, FVector::RightVector, FVector::ForwardVector, false);
}

FString UERShape_Line::Describe() const
{
	return FString::Printf(TEXT("Line(길이 %.1f · 폭 %.2f)"), Length, Width);
}

// ── 사다리꼴 ───────────────────────────────────────────────
void UERShape_Trapezoid::FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	Q.Shape = ESkillTargeting::Trapezoid;
	Q.TrapLength = Length;
	Q.TrapNearWidth = NearWidth;
	Q.TrapFarWidth = FarWidth;
	Q.RangeMax = GetMaxReach();
}

void UERShape_Trapezoid::Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const
{
	const FVector O = Up(Q.Origin);
	const FVector F = Q.Direction.GetSafeNormal2D();
	const FVector Rt(-F.Y, F.X, 0.f);
	const float Half = Length * 50.f;   // 원점이 가운데
	const FVector N1 = O - F * Half + Rt * NearWidth * 50.f, N2 = O - F * Half - Rt * NearWidth * 50.f;
	const FVector F1 = O + F * Half + Rt * FarWidth * 50.f, F2 = O + F * Half - Rt * FarWidth * 50.f;
	DrawDebugLine(World, N1, N2, Color, false, Life, 0, 2.f);
	DrawDebugLine(World, N1, F1, Color, false, Life, 0, 2.f);
	DrawDebugLine(World, N2, F2, Color, false, Life, 0, 2.f);
	DrawDebugLine(World, F1, F2, Color, false, Life, 0, 2.f);
}

FString UERShape_Trapezoid::Describe() const
{
	return FString::Printf(TEXT("Trapezoid(%s · 길이 %.1f · 폭 %.1f→%.1f)"),
		ShapeOrigin == EERShapeOrigin::AimPoint ? *FString::Printf(TEXT("조준점 %.1f"), AimRange) : TEXT("시전자"), Length, NearWidth, FarWidth);
}

// ── 플레이어마다 원 ────────────────────────────────────────
void UERShape_PlayerCircles::FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	Q.Shape = ESkillTargeting::GroundCircle;
	Q.RadiusOuter = Radius;
	Q.RangeMax = CaptureRange;
}

void UERShape_PlayerCircles::CaptureAtCastStart(const FERShapeContext& Ctx, TArray<FVector>& OutPoints) const
{
	// 지금 근처에 있는 실험체 자리를 **팀 상관없이 전부** (사용자 2026-10-01 "팀 상관없이 근처 모든 플레이어한테")
	const AActor* Me = Ctx.Avatar;
	const AGameStateBase* GS = Me ? Me->GetWorld()->GetGameState() : nullptr;
	const float MaxSq = FMath::Square(CaptureRange * 100.f);
	for (const APlayerState* PS : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		const APawn* P = PS ? PS->GetPawn() : nullptr;
		const UAbilitySystemComponent* PASC = P ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(P) : nullptr;
		if (PASC && PASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) > 0.f
			&& FVector::DistSquared2D(P->GetActorLocation(), Me->GetActorLocation()) <= MaxSq)
		{
			OutPoints.Add(ERTargeting::GetTargetingLocation(P));
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 원 %d개 자리 저장 (반경 %.1fm 안 실험체 전원)"), *GetNameSafe(Me), OutPoints.Num(), CaptureRange);
}

FTargetResult UERShape_PlayerCircles::Query(const UWorld* World, const FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	FTargetResult Result;
	if (!Ctx.CapturedPoints)
	{
		return Result;
	}
	for (const FVector& P : *Ctx.CapturedPoints)   // 저장한 자리마다 지면 원 → 합친다 (같은 액터는 한 번)
	{
		FTargetQuery C = Q;
		C.Origin = P;
		const FTargetResult One = ERTargeting::Query(World, C);
		for (AActor* A : One.HitActors) { if (A) { Result.HitActors.AddUnique(A); } }
	}
	return Result;
}

void UERShape_PlayerCircles::Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const
{
	Circle(World, Q.Instigator ? Q.Instigator->GetActorLocation() : Q.Origin, CaptureRange * 100.f, FColor(Color.R, Color.G, Color.B, 80), Life, 1.f);
}

FString UERShape_PlayerCircles::Describe() const
{
	return FString::Printf(TEXT("PlayerCircles(저장 %.1f · 원 %.1f)"), CaptureRange, Radius);
}
