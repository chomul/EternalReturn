// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Shape/ERSkillShape.h"

#include "Combat/ERTargeting.h"
#include "GAS/ERSkillData.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FTargetQuery UERSkillShapeBase::BuildQuery(const FERShapeContext& Ctx) const
{
	FTargetQuery Q;
	if (Ctx.Targets)
	{
		Q.TeamFilter = Ctx.Targets->Team;
		Q.bPlayersOnly = Ctx.Targets->bPlayersOnly;
		Q.MaxTargets = Ctx.Targets->MaxTargets;
	}
	Q.Instigator = Ctx.Avatar;
	Q.Direction = Ctx.AimDirection;
	Q.DesignatedTarget = Ctx.DesignatedTarget;
	Q.RangeMin = MinReach;
	Q.Origin = ShapeOrigin == EERShapeOrigin::AimPoint ? Ctx.AimPoint : ERTargeting::GetTargetingLocation(Ctx.Avatar);
	// 원을 조준 방향 앞으로 — 곰 강타 "앞발이 내려친 자리" (F12.6-04 · 사용자 2026-09-30)
	if (GetForwardOffset() > 0.f && ShapeOrigin == EERShapeOrigin::Caster)
	{
		const FVector Fwd = Ctx.AimDirection.GetSafeNormal2D().IsNearlyZero()
			? (Ctx.Avatar ? Ctx.Avatar->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector)
			: Ctx.AimDirection.GetSafeNormal2D();
		Q.Origin += Fwd * GetForwardOffset() * 100.f;
	}
	FillQuery(Q, Ctx);
	if (Ctx.RangeOverride > 0.f)
	{
		Q.RangeMax = Ctx.RangeOverride;
	}
	return Q;
}

FTargetResult UERSkillShapeBase::Query(const UWorld* World, const FTargetQuery& Q, const FERShapeContext& Ctx) const
{
	return ERTargeting::Query(World, Q);
}

FString UERSkillShapeBase::Describe() const
{
	return FString::Printf(TEXT("%s(%s%s · 최소 %.1f)"), *GetClass()->GetName().Replace(TEXT("ERShape_"), TEXT("")),
		ShapeOrigin == EERShapeOrigin::AimPoint ? TEXT("조준점 ") : TEXT("시전자"),
		ShapeOrigin == EERShapeOrigin::AimPoint ? *FString::Printf(TEXT("%.1f"), AimRange) : TEXT(""), MinReach);
}

#if WITH_EDITOR
void UERSkillShapeBase::ValidateShape(FDataValidationContext& Context, const FString& Owner) const
{
	// 하한 > 상한이면 아무것도 못 맞힌다 — 조용히 죽는다 (2026-09-14 RangeMin 5 · 적중 0 × 6)
	if (MinReach > GetMaxReach())
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s: 최소 거리 %.1f > 최대 %.1f — 아무것도 못 맞힌다"), *Owner, MinReach, GetMaxReach())));
	}
	if (ShapeOrigin == EERShapeOrigin::AimPoint && AimRange <= 0.f)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s: 원점이 조준점인데 AimRange 0 — 조준점이 늘 발밑"), *Owner)));
	}
}
#endif
