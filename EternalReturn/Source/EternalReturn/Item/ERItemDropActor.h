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
 */
UCLASS()
class ETERNALRETURN_API AERItemDropActor : public AActor
{
	GENERATED_BODY()

public:
	AERItemDropActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** [서버] 내용을 채운다. 스폰 직후 한 번. */
	void Initialize(int32 InDropId, const TArray<FERItemInstance>& InItems);

	/**
	 * [서버] 칸에서 Count 만큼 뺀다. 실제로 뺀 수량을 돌려준다 (칸이 비었으면 0).
	 * 다 비면 스스로 Destroy — 부르는 쪽은 그 뒤 this 를 만지지 않는다.
	 */
	int32 TakeFromSlot(int32 Index, int32 Count);

	const TArray<FERItemInstance>& GetItems() const { return Items; }
	int32 GetDropId() const { return DropId; }

protected:
	/** 서버가 발급하는 번호. 디버그 · 로그가 액터를 가리킬 때 쓴다 (액터 이름은 머신마다 다르다). */
	UPROPERTY(Replicated)
	int32 DropId = 0;

	UPROPERTY(Replicated)
	TArray<FERItemInstance> Items;
};
