// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Wilson.h"

#include "Combat/ERWilson.h"
#include "EternalReturn.h"

void UERSkillFragment_WilsonThrow::OnExecute(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority)
	{
		return;
	}
	if (AERWilson* Wilson = AERWilson::FindFor(Ctx.Avatar); Wilson && Wilson->IsPullingOwner())
	{
		Wilson->HoldPullForThrow();
	}
}

namespace
{
	/** 윌슨 액터는 복제된다 (주인 = 시셀라) — 소유 클라도 찾는다 */
	bool WilsonPreviewOrigin(const AActor* Avatar, FVector& OutOrigin)
	{
		if (const AERWilson* Wilson = AERWilson::FindFor(Avatar))
		{
			OutOrigin = Wilson->GetActorLocation();
			return true;
		}
		return false;
	}
}

bool UERSkillFragment_WilsonThrow::GetPreviewOrigin(const AActor* Avatar, FVector& OutOrigin, bool& bOutFullReach) const
{
	bOutFullReach = false;   // 조준점까지 (조준점은 시셀라 기준 사거리 안)
	return WilsonPreviewOrigin(Avatar, OutOrigin);
}

bool UERSkillFragment_WilsonTether::GetPreviewOrigin(const AActor* Avatar, FVector& OutOrigin, bool& bOutFullReach) const
{
	bOutFullReach = true;    // 윌슨에서 커서 쪽으로 사거리 5.5m 만큼 (AERProjectile_WilsonTether 와 같다)
	return WilsonPreviewOrigin(Avatar, OutOrigin);
}

void UERSkillFragment_WilsonJoin::OnExecute(FERSkillContext& Ctx) const
{
	if (!Ctx.bAuthority || Ctx.bActivatedByRecast)
	{
		return;
	}
	if (AERWilson* Wilson = AERWilson::FindFor(Ctx.Avatar))
	{
		Wilson->Join(TEXT("W"));
	}
	else
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] %s W — 이미 붙어 있음 (합침 아님 · 장전 없음)"), *GetNameSafe(Ctx.Avatar));
	}
}
