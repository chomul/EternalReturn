// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ERProjectileBase.h"
#include "ERProjectile_Bike.generated.h"

/**
 * 발사한 바이크 (F19-02 매그너스 R 재사용 · Argument 62) — 베이스 직선 비행 + **벽에서 멈춤** + 적 · 벽에 닿으면 **폭발**.
 * 폭발 설정은 쏜 데이터(재사용 DA)의 탑승 조각 (ExplodeRadius · ExplodeSkill). 투사체는 원래 벽을 통과하지만(카티야 · 원작) 바이크만 부딪힌다.
 */
UCLASS()
class ETERNALRETURN_API AERProjectile_Bike : public AERProjectileBase
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** 비행 시작(서버 · 클라) — 쏜 사람 스킨의 바이크 소품(Pres.Prop.Bike)을 몸에 붙인다 (Argument 64 B1 · 각 머신 로컬) */
	virtual void StartFlight() override;
	virtual void OnHitTarget(AActor* Target) override;

private:
	void Explode(const FVector& Where, const TCHAR* Why);

	/** 붙인 바이크 조각 (이 머신만) */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> BikeComps;
};
