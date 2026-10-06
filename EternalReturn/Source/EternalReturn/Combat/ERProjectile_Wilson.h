// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ERProjectileBase.h"
#include "ERProjectile_Wilson.generated.h"

/**
 * 시셀라 Q — 날아가는 윌슨 (F19-04 · Argument 68).
 * 윌슨이 **떨어져 있으면 그 자리에서** 조준점으로 (조준점은 시셀라 기준 사거리 안 — 오리아나 Q · 사용자 2026-10-06). 붙어 있으면 시셀라에게서.
 * 길의 적 = 기본 투사체 판정 (관통 · Q DA 피해 조각). 끝나면(조준점 · 벽) [서버] 그 자리에서
 * 착지 범위 피해 (UERSkillFragment_WilsonThrow::LandSkill · LandRadius) → 윌슨을 내려놓는다.
 */
UCLASS()
class ETERNALRETURN_API AERProjectile_Wilson : public AERProjectileBase
{
	GENERATED_BODY()

public:
	virtual void InitLaunch(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, const FTargetQuery& InFilter, const FERProjectileLaunch& InLaunch, int32 InShotIndex = -1, bool bInPierce = false) override;

protected:
	virtual void EndFlight(const TCHAR* Why) override;

private:
	void Land();
};

/**
 * 시셀라 E — 윌슨이 몸을 뻗는다 (F19-04 · Argument 68).
 * 윌슨이 떨어져 있으면 **윌슨 자리에서** 커서 쪽으로 · 붙어 있으면 시셀라에게서 (기본).
 * [서버] 적 적중 = 기본(피해 · 기절 조각) + 윌슨(또는 시셀라) 쪽으로 끌어옴 · 시셀라 적중(윌슨이 떨어져 있을 때만) = 끌려감 · 보호막 · 쿨 감소.
 */
UCLASS()
class ETERNALRETURN_API AERProjectile_WilsonTether : public AERProjectileBase
{
	GENERATED_BODY()

public:
	virtual void InitLaunch(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, const FTargetQuery& InFilter, const FERProjectileLaunch& InLaunch, int32 InShotIndex = -1, bool bInPierce = false) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnHitTarget(AActor* Target) override;

private:
	void HitSissela();

	/** 윌슨 자리에서 뻗었나 (떨어져 있었나) — 시셀라를 맞힐 수 있는 건 이때뿐 */
	bool bFromWilson = false;
	/** 끌어올 자리 (윌슨 · 또는 시셀라) */
	FVector PullTo = FVector::ZeroVector;
};
