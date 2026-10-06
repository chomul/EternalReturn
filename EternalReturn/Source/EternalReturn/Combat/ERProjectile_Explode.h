// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ERProjectileBase.h"
#include "ERProjectile_Explode.generated.h"

/**
 * 끝나면(적중 · 사거리 · 벽) 그 자리에서 폭발하는 투사체 (레니 Q · Argument 70).
 * 폭발 데이터 · 반경은 스킬 DA 의 `끝에서 폭발` 조각. 맞은 대상 자체의 적중 조각은 기본(Q DA 에 적중 조각이 없으면 아무 일 없음).
 */
UCLASS()
class ETERNALRETURN_API AERProjectile_Explode : public AERProjectileBase
{
	GENERATED_BODY()

protected:
	virtual void EndFlight(const TCHAR* Why) override;
};
