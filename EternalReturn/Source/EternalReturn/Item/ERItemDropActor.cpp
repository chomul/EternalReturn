// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/ERItemDropActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EternalReturn.h"
#include "Core/ERPlayerState.h"
#include "GAS/ERGameplayTags.h"
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

void AERItemDropActor::InitializeCorpse(FName InLootRow, float Lifetime, bool bInKeepWhenEmpty, int32 SourceLevel)
{
	InitializeFromLoot(InLootRow, SourceLevel);
	if (!HasAuthority())
	{
		return;
	}
	// 상자와 다른 점은 **수명**이다. 비었을 때 사라질지는 부르는 쪽이 정한다 —
	// 야생동물 시체는 열어서 필요한 것만 가져가는 것이라 비어도 남고(true), 시간이 되면 몸과 같이 사라진다 (사용자 2026-09-23).
	bKeepWhenEmpty = bInKeepWhenEmpty;
	bCorpse = true;   // 상자가 아니다 — 탐색 숙련도 대상에서 뺀다
	if (Lifetime > 0.f)
	{
		SetLifeSpan(Lifetime);
	}
}

void AERItemDropActor::InitializeFromLoot(FName InLootRow, int32 SourceLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	LootRow = InLootRow;
	bKeepWhenEmpty = true;   // 상자 · 채집물 — 비어도 남는다 (시체는 InitializeCorpse 가 되돌린다)
	if (DropId == 0)
	{
		DropId = NextDropId();
	}
	const FERLootRow* Row = ERLoot::Find(LootRow);   // 없으면 Error 로그
	Items.Reset();
	if (Row)
	{
		ERLoot::Roll(*Row, Items, SourceLevel);   // 시체는 동물 레벨 (레벨 조건 드랍) · 상자는 0
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
		// ⭐ 채집 포즈 (F12.5-04 · Argument 46) — 저격 모드와 같은 길: 서버가 복제 loose 태그 → 각 머신 AnimBP. 연출 전용 태그, 아무것도 막지 않는다.
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Who);
		if (AERPlayerState* PS = Cast<AERPlayerState>(Who))
		{
			PS->SetGatherSeconds(OutSeconds);   // 포즈 애니를 이 시간에 맞춘다 (Argument 47 E2) — 태그보다 먼저
		}
		if (ASC && !bGatherTagged)
		{
			ASC->AddLooseGameplayTag(ERTags::State_Gathering);
			ASC->AddReplicatedLooseGameplayTag(ERTags::State_Gathering);
			bGatherTagged = true;
		}
		// 안전망 — 인벤토리 쪽 타이머가 못 끝내도 잠금이 영원히 남지 않게. 인벤토리가 먼저 EndGather 하면 이건 무해.
		GetWorldTimerManager().SetTimer(GatherTimer, this, &AERItemDropActor::EndGather, OutSeconds + 1.f, false);
	}
	return true;
}

void AERItemDropActor::EndGather()
{
	if (bGatherTagged)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Gatherer.Get()))
		{
			ASC->RemoveLooseGameplayTag(ERTags::State_Gathering);
			ASC->RemoveReplicatedLooseGameplayTag(ERTags::State_Gathering);
		}
		bGatherTagged = false;
	}
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
	if (bEmpty && !bKeepWhenEmpty)
	{
		// 시체(플레이어 · 야생동물)는 다 가져가면 사라진다. ⭐ 상자는 **비어도 남는다** (원작 확인 2026-09-23 · 역기획서 몬스터 §3.3).
		UE_LOG(LogEternalReturn, Log, TEXT("[드롭] #%d 가 비어 사라진다."), DropId);
		Destroy();
	}
	return Taken;
}

bool AERItemDropActor::MarkOpenedByFirst()
{
	// ⚠ 야생동물 시체는 **상자가 아니다** — 보상은 사냥 숙련도이고, 여기서 탐색 숙련도까지 주면 이중이다
	//   (2026-09-23 로그: 닭 시체를 열었는데 `Search +100 (상자 Wild_Chicken)`).
	if (LootRow.IsNone() || IsInfinite() || bOpenedOnce || bCorpse)
	{
		return false;
	}
	bOpenedOnce = true;
	return true;
}
