// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item/ERInventoryComponent.h"
#include "ERItemDropActor.generated.h"

/**
 * 시체 — 사망 시 떨어진 아이템 **컨테이너 하나** (장비 역기획서 §7.3 권장, 아이템당 액터가 아님).
 *
 * ⭐ 서버가 스폰하고 내용은 전원에게 복제된다 — 시체 안은 누구나 본다.
 * ⭐ 내용은 **복사본**이다 (원작 확인: 남이 주워가도 죽은 사람 것은 그대로). 원본 인벤토리와 연결이 없다.
 * ⭐ 습득은 UERInventoryComponent::ServerPickup → 서버가 거리 · 존재 · 여유를 검증하고 이 액터의 칸에서 뺀다.
 *   두 명이 같은 칸을 노리면 **서버 RPC 처리 순서**가 승자다 — 나중 요청은 칸이 비어 거절. 클라 판정 없음 (§7.4).
 *
 * 릴리번시: 값이 바뀔 때만 깨운다 (DORM_DormantAll + FlushNetDormancy). 비면 Destroy.
 *
 * ⭐ F09-03: 상자 · 채집물도 **이 클래스**다 — "미리 채워진 시체". LootRow 를 지정해 맵에 놓으면(또는 스폰하면) 서버가 BeginPlay 에서 루트를 굴려 채운다.
 *   상자는 한 번 가져가면 끝(비면 Destroy). 채집물(bInfinite)은 줄지 않고, GatherSeconds 동안 **한 명만** 캔다 (서버 점유 잠금). 습득 경로는 그대로 ServerPickup.
 */
UCLASS()
class ETERNALRETURN_API AERItemDropActor : public AActor
{
	GENERATED_BODY()

public:
	AERItemDropActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** [서버] 시체 내용을 채운다. 스폰 직후 한 번. DropLifetime 이 걸린다. */
	void Initialize(int32 InDropId, const TArray<FERItemInstance>& InItems);

	/** [서버] 루트 행으로 채운다 (상자 · 채집물). 스폰 때 한 번. 수명 없음. */
	void InitializeFromLoot(FName InLootRow);

	/** 채집물인가 (LootRow 의 bInfinite). 클라도 행을 읽을 수 있다. */
	bool IsInfinite() const;

	/**
	 * [서버] 채집 점유 시도 (F09-03). 비어 있거나 본인이면 true 로 잠그고, 다른 사람이 캐는 중이면 false.
	 * 잠금은 GatherSeconds 뒤 자동 해제 — 그때 UERInventoryComponent 가 아이템을 준다. 이동 취소는 F17 상호작용 때.
	 */
	bool TryBeginGather(class APlayerState* Who, float& OutSeconds);
	void EndGather();
	APlayerState* GetGatherer() const { return Gatherer.Get(); }

	/** 서버가 발급하는 다음 번호. 시체 · 상자 공용. */
	static int32 NextDropId();

	/**
	 * 루트 행 (F09-03). 비어 있으면 시체(코드가 Initialize 로 채운다). 맵에 놓을 때 지정 — "Box_Basic" · "Gather_Branch" …
	 * ⚠ 지역별 종류는 행 이름으로 — 코드에 지역 분기 없음.
	 */
	UPROPERTY(EditAnywhere, Category = "루트")
	FName LootRow;

	/**
	 * [서버] 칸에서 Count 만큼 뺀다. 실제로 뺀 수량을 돌려준다 (칸이 비었으면 0).
	 * 다 비면 스스로 Destroy — 부르는 쪽은 그 뒤 this 를 만지지 않는다.
	 */
	int32 TakeFromSlot(int32 Index, int32 Count);

	const TArray<FERItemInstance>& GetItems() const { return Items; }
	int32 GetDropId() const { return DropId; }

protected:
	virtual void BeginPlay() override;

	/** 서버 전용. 지금 캐고 있는 사람 — 한 번에 한 명 (원작 확인). 복제 안 함 — 표시는 F17. */
	TWeakObjectPtr<APlayerState> Gatherer;
	FTimerHandle GatherTimer;

	/** 서버가 발급하는 번호. 디버그 · 로그가 액터를 가리킬 때 쓴다 (액터 이름은 머신마다 다르다). */
	UPROPERTY(Replicated)
	int32 DropId = 0;

	UPROPERTY(Replicated)
	TArray<FERItemInstance> Items;
};
