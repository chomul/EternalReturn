// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERProjectile_Homing.h"

#include "GameFramework/ProjectileMovementComponent.h"

void AERProjectile_Homing::Tick(float DeltaSeconds)
{
	if (bFlying && !bEnded)
	{
		if (const AActor* Target = Launch.HomingTarget.Get())
		{
			// 대상 몸 가운데로 — 속도 크기는 그대로, 방향만
			const FVector Dir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			if (!Dir.IsNearlyZero())
			{
				Movement->Velocity = Dir * Launch.SpeedUU;
			}
		}
		if (HasAuthority() && GetGameTimeSinceCreation() > MaxFlightSeconds)
		{
			EndFlight(TEXT("시간 초과"));
			return;
		}
	}
	Super::Tick(DeltaSeconds);
}
