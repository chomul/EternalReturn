// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ERProjectileBase.h"
#include "ERProjectile_Homing.generated.h"

/**
 * **따라가는** 투사체 (F19-01 카티야 R · Argument 54 — 베이스 상속).
 * 매 틱 대상 쪽으로 속도를 돌린다 — 대상이 순간이동해도 끝까지 (사용자 2026-10-01 "스캔 되었으면 무조건 따라감").
 * 판정은 베이스 그대로 — 지나는 길의 **다른 적이 대신 맞을 수 있다** (게임 툴팁). 사거리 대신 최대 비행 시간.
 * 서버 · 클라 둘 다 같은 대상(복제된 HomingTarget)을 따라 스스로 그린다.
 */
UCLASS()
class ETERNALRETURN_API AERProjectile_Homing : public AERProjectileBase
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** 이만큼 날아도 못 맞히면 끝 (초) `[자체]` — 대상이 사라졌을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "투사체", meta = (ClampMin = "0.1"))
	float MaxFlightSeconds = 3.f;
};
