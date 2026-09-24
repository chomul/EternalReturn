// Copyright Epic Games, Inc. All Rights Reserved.

#include "Wildlife/ERWildlifeSpawnPoint.h"

#include "Components/BillboardComponent.h"

AERWildlifeSpawnPoint::AERWildlifeSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;   // 서버만 읽는다 — 클라는 스폰된 동물만 받는다

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	// 에디터에서 자리가 보이게 — 게임에는 없다.
	if (UBillboardComponent* Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite")))
	{
		Sprite->SetupAttachment(RootComponent);
	}
#endif
}

FVector AERWildlifeSpawnPoint::GetSpawnLocation(int32 Index) const
{
	const FVector Center = GetActorLocation();
	if (Count <= 1 || ScatterRadius <= 0.f)
	{
		return Center;
	}
	// 무리 — 원 위에 고르게 놓고 약간 흔든다 (같은 자리에 겹쳐 스폰되지 않게).
	const float Angle = (2.f * PI * Index) / Count + FMath::FRandRange(-0.3f, 0.3f);
	const float Dist = ScatterRadius * FMath::FRandRange(0.5f, 1.f);
	return Center + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
}
