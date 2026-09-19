// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/ERItemDropActor.h"

#include "EternalReturn.h"
#include "Item/ERItemSettings.h"
#include "Item/ERLootLibrary.h"
#include "Item/ERLootTypes.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
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

int32 AERItemDropActor::NextDropId()
{
	static int32 Next = 1;   // 서버 프로세스 안에서만 의미 있는 번호. 로그 · 디버그용
	return Next++;
}

void AERItemDropActor::BeginPlay()
{
	Super::BeginPlay();
	// 맵에 놓인 상자 · 채집물 — 서버가 채운다. 시체는 LootRow 가 비어 있어 여기서 아무것도 안 한다.
	if (HasAuthority() && !LootRow.IsNone())
	{
		InitializeFromLoot(LootRow);
	}
}

void AERItemDropActor::InitializeFromLoot(FName InLootRow)
{
	if (!HasAuthority())
	{
		return;
	}
	LootRow = InLootRow;
	if (DropId == 0)
	{
		DropId = NextDropId();
	}
	const FERLootRow* Row = ERLoot::Find(LootRow);   // 없으면 Error 로그
	Items.Reset();
	if (Row)
	{
		ERLoot::Roll(*Row, Items);
	}
	FlushNetDormancy();

	FString List;
	for (const FERItemInstance& I : Items) { List += FString::Printf(TEXT(" %s×%d"), *I.ItemId.ToString(), I.Count); }
	UE_LOG(LogEternalReturn, Log, TEXT("[루트] #%d %s 채움:%s"), DropId, *LootRow.ToString(), Items.IsEmpty() ? TEXT(" (없음)") : *List);
}

bool AERItemDropActor::IsInfinite() const
{
	const FERLootRow* Row = LootRow.IsNone() ? nullptr : ERLoot::Find(LootRow);
	return Row && Row->bInfinite;
}

bool AERItemDropActor::TryBeginGather(APlayerState* Who, float& OutSeconds)
{
	OutSeconds = 0.f;
	if (!HasAuthority() || !Who)
	{
		return false;
	}
	if (Gatherer.IsValid() && Gatherer.Get() != Who)
	{
		return false;   // ⭐ 한 번에 한 명 (원작 확인)
	}
	const FERLootRow* Row = ERLoot::Find(LootRow);
	OutSeconds = Row ? Row->GatherSeconds : 0.f;
	Gatherer = Who;
	if (OutSeconds > 0.f)
	{
		// 안전망 — 인벤토리 쪽 타이머가 못 끝내도 잠금이 영원히 남지 않게. 인벤토리가 먼저 EndGather 하면 이건 무해.
		GetWorldTimerManager().SetTimer(GatherTimer, this, &AERItemDropActor::EndGather, OutSeconds + 1.f, false);
	}
	return true;
}

void AERItemDropActor::EndGather()
{
	Gatherer.Reset();
	GetWorldTimerManager().ClearTimer(GatherTimer);
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

	// ⭐ 채집물은 줄지 않는다 (원작 확인: 개수 제한 없음). 내용 복제도 안 바뀐다.
	if (IsInfinite())
	{
		return Taken;
	}

	Items[Index].Count -= Taken;
	if (Items[Index].Count <= 0)
	{
		Items[Index] = FERItemInstance();
	}
	FlushNetDormancy();

	const bool bEmpty = !Items.ContainsByPredicate([](const FERItemInstance& I) { return !I.IsEmpty(); });
	if (bEmpty)
	{
		// 상자 · 시체는 한 번 가져가면 끝 (원작 확인).
		UE_LOG(LogEternalReturn, Log, TEXT("[드롭] #%d 가 비어 사라진다."), DropId);
		Destroy();
	}
	return Taken;
}
