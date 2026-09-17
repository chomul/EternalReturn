// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/ERItemDropActor.h"

#include "EternalReturn.h"
#include "Item/ERItemSettings.h"
#include "Net/UnrealNetwork.h"

AERItemDropActor::AERItemDropActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 스폰 후 내용이 바뀌는 건 습득 때뿐 → 평소엔 재운다. 바뀔 때 FlushNetDormancy (§7.3 릴리번시).
	NetDormancy = DORM_DormantAll;

	// 메시 · 콜리전은 연출 단계에서. 지금은 위치만 있는 액터다.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AERItemDropActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AERItemDropActor, DropId);
	DOREPLIFETIME(AERItemDropActor, Items);
}

void AERItemDropActor::Initialize(int32 InDropId, const TArray<FERItemInstance>& InItems)
{
	DropId = InDropId;
	Items = InItems;

	const float Lifetime = UERItemSettings::Get().DropLifetime;
	if (Lifetime > 0.f)
	{
		SetLifeSpan(Lifetime);   // 0 이면 정리 안 함 — 자체 결정값 (미확인)
	}
	FlushNetDormancy();
}

int32 AERItemDropActor::TakeFromSlot(int32 Index, int32 Count)
{
	if (!HasAuthority() || !Items.IsValidIndex(Index) || Items[Index].IsEmpty() || Count <= 0)
	{
		return 0;
	}

	const int32 Taken = FMath::Min(Items[Index].Count, Count);
	Items[Index].Count -= Taken;
	if (Items[Index].Count <= 0)
	{
		Items[Index] = FERItemInstance();
	}
	FlushNetDormancy();

	const bool bEmpty = !Items.ContainsByPredicate([](const FERItemInstance& I) { return !I.IsEmpty(); });
	if (bEmpty)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[드롭] 시체 #%d 가 비어 사라진다."), DropId);
		Destroy();
	}
	return Taken;
}
