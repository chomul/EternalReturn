// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Item/ERItemTypes.h"
#include "ERInventoryComponent.generated.h"

class UAbilitySystemComponent;
class AERItemDropActor;

/**
 * 가방 칸 하나 = 아이템 **인스턴스** (Argument 20 정의/인스턴스 분리). 비면 ItemId = None.
 * 실시간 상태(수량 · 나중의 강화)는 여기에. 정의는 FERItemRow.
 */
USTRUCT()
struct FERItemInstance
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemId;

	UPROPERTY()
	int32 Count = 0;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
};

/** 장착 칸 하나. 클라에는 ItemId 만 간다 — 핸들은 서버 것. */
USTRUCT()
struct FEREquippedSlot
{
	GENERATED_BODY()

	UPROPERTY()
	EEREquipSlot Slot = EEREquipSlot::None;

	UPROPERTY()
	FName ItemId;

	/** 서버 전용. 이 장비의 스탯 GE. 해제 = 이 핸들 제거. */
	FActiveGameplayEffectHandle EffectHandle;
};

/**
 * 인벤토리 — **PlayerState 에 붙는다** (Docs/4_Argument/21_인벤토리_소유주체.md 방안 B).
 *   ASC 와 같은 액터라 장비 GE 와 핸들의 수명이 같다. 폰이 죽고 살아도 어긋나지 않는다.
 *
 * F08-03 범위: **장착 5칸** + 장비 스탯 GE. 가방(칸 · 수량 · COND_OwnerOnly) 은 F08-04, 드롭 · 습득은 F08-05.
 *
 * ⭐ 장착 · 해제는 **서버 권위.** 클라는 RPC 로 요청만 하고, 서버가 ERItem::CanEquip 으로 판정한다 — 클라 검사를 믿지 않는다.
 * ⭐ 장비 스탯은 UEREquipmentEffect(Infinite) 하나를 아이템 값으로 적용. 해제는 핸들 제거뿐 — 수동 차감 없음.
 * ⚠ 무기 교체 시 D 스킬 교체는 F11 (TakeSkills / GrantSkills 자리는 있다).
 */
UCLASS(ClassGroup = (ER), meta = (BlueprintSpawnableComponent))
class ETERNALRETURN_API UERInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UERInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** [서버] 장착 칸이 바뀐 뒤 (장착 · 해제 · 교체). F10-04 숙련도 증폭 · F11 D 교체가 받는다. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquippedChanged, EEREquipSlot /*Slot*/);
	FOnEquippedChanged OnEquippedChanged;

	// ── 가방 (F08-04) ──────────────────────────────────────────

	/**
	 * [서버] 가방에 넣는다. 같은 아이템의 스택부터 채우고(MaxStack), 그 다음 빈 칸.
	 * @return **넣지 못한 수량.** 0 이면 전부 들어갔다. 가득 차면 남는 만큼 돌려준다 — 부르는 쪽(습득 F08-05)이 땅에 남긴다. 자체 결정값.
	 */
	int32 AddItem(FName ItemId, int32 Count = 1);

	/** [서버] 가방 칸에서 뺀다. 칸이 비거나 수량이 모자라면 false (아무것도 안 뺀다). */
	bool RemoveItem(int32 BagIndex, int32 Count = 1);

	/** 이 수량이 다 들어가는가 (스택 여유 + 빈 칸). 서버 · 클라(복제된 가방) 모두. */
	bool HasSpaceFor(FName ItemId, int32 Count = 1) const;

	const TArray<FERItemInstance>& GetBag() const { return Bag; }

	// ── 장착 ──────────────────────────────────────────────────

	/** [클라 -> 서버] 가방 칸의 장비를 장착. 그 슬롯에 있던 장비는 가방으로 돌아온다 — 자리 없으면 거부. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerEquipFromBag(int32 BagIndex);

	/** [클라 -> 서버] 해제 → 가방으로. 가방에 자리 없으면 거부 (아이템이 증발하면 안 된다). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUnequip(EEREquipSlot Slot);

	/** [클라 -> 서버] 가방 칸 버리기. 드롭 액터는 F08-05 — 지금은 제거만. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerDiscard(int32 BagIndex);

	/** [서버] 가방 칸 → 장착. 성공하면 true. */
	bool EquipFromBag(int32 BagIndex);

	/** [서버] 해제 → 가방. 자리 없으면 false. */
	bool Unequip(EEREquipSlot Slot);

	/**
	 * [서버] 가방을 거치지 않는 직접 장착 (디버그 · 초기 장비). 있던 장비는 **버려진다** — 게임 경로는 EquipFromBag.
	 */
	bool EquipDirect(FName ItemId);

	// ── 드롭 · 습득 (F08-05) ───────────────────────────────────

	/**
	 * [서버] 사망 시 시체를 만든다. ⭐ **복사**다 — 원본(가방 · 장착)은 건드리지 않는다 (원작 확인: 남이 주워가도 본인 것은 그대로).
	 * 규칙: 재료(비장비) 전부 · 장비는 **영웅 이하** (전설 · 초월 제외 — 원작 확인). 영웅 미만 장비 포함은 자체 결정값.
	 * 남길 것이 없으면 스폰하지 않는다. AERPlayerState 가 OnOutOfHealth 에서 부른다.
	 */
	AERItemDropActor* SpawnDeathDrop(const FVector& Location);

	/**
	 * [클라 -> 서버] 시체 · 상자 · 채집물의 칸 하나를 줍는다. 서버 검증: 액터 유효 · 거리(UERItemSettings.PickupRange) · 칸에 아직 있음 · 가방 여유.
	 * 들어간 만큼만 시체에서 뺀다. ⭐ 경쟁은 서버 RPC 순서 — 클라 선착순 판정 없음 (§7.4). 낙관적 표시 금지.
	 * 채집물(F09-03)은 줄지 않고, 한 번에 한 명만 — 점유 뒤 GatherSeconds 후 FinishGather 가 준다.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPickup(AERItemDropActor* Drop, int32 Index);

	/** [서버] 채집 완료 — GatherSeconds 뒤 타이머가 부른다. 점유 해제 + 아이템 지급 (F09-03). */
	void FinishGather(AERItemDropActor* Drop, int32 Index);
	FTimerHandle GatherTimer;

	/** 장착 중인 아이템 (서버 · 클라 모두 — 복제됨). 없으면 NAME_None. */
	FName GetEquippedItem(EEREquipSlot Slot) const;

	// ── 제작 (F09-02) ──────────────────────────────────────────

	/** 가방 + 장착 합산 수량 (재료 검사 · UI 회색 처리). 서버 · 클라(복제된 만큼) 모두. */
	int32 CountItem(FName ItemId) const;

	/**
	 * [서버] 제작 완료 알림 — 결과 ID · 이 플레이어가 그 아이템을 **처음** 만들었나. F10-04 가 받아 무기 숙련도(등급별 100~800 · 최초 +25%)를 준다.
	 * 인벤토리는 성장을 모른다 (OnEquippedChanged 와 같은 결).
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnItemCrafted, FName /*ResultId*/, bool /*bFirstTime*/);
	FOnItemCrafted OnItemCrafted;

	/**
	 * [클라 -> 서버] "ResultId 를 만들겠다". 서버가 재료 · 자리를 전부 검사한 뒤에만 바꾼다 — 부분 실패 없음.
	 * 재료는 가방 우선, 없으면 **장착 중인 것**도 쓴다. 장착 재료를 썼고 결과가 장비면 그 슬롯에 바로 장착 — 자체 결정값 (원작 (미확인)).
	 * 한 번에 한 조합 — 연쇄는 UI 가 트리(ERCraft::ExpandLeaves)를 보고 반복 요청한다.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerCraft(FName ResultId);

	/** [서버] ServerCraft 의 몸통. 성공하면 true. 실패는 아무것도 바꾸지 않는다. */
	bool Craft(FName ResultId);

	/** 서버 전용 — 핸들 확인용 (디버그). */
	const FEREquippedSlot* FindEquippedSlot(EEREquipSlot Slot) const;

protected:
	UAbilitySystemComponent* GetASC() const;

	/** 장착에 쓰는 내부 몸통. 장착 성공 시 true. 실패하면 아무것도 바꾸지 않는다. */
	bool ApplyEquip(const FName ItemId, const struct FERItemRow& Item);

	/**
	 * 장착 칸. 전원에게 복제 — 남이 내 무기를 봐야 한다 (F11 무기 메시). 5칸이라 가볍다.
	 * ⚠ 가방은 이것과 별도 배열이고 COND_OwnerOnly 다.
	 */
	UPROPERTY(Replicated)
	TArray<FEREquippedSlot> Equipped;

	/**
	 * 가방. **`COND_OwnerOnly`** — 남의 가방을 복제하면 정보 유출이다 (UI_HUD 역기획서 :101).
	 * 크기 = UERItemSettings.InventorySlots (원작 10). BeginPlay 에서 서버가 채운다. 인덱스 = 칸.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Bag)
	TArray<FERItemInstance> Bag;

	/** 서버 전용. 이 플레이어가 한 번이라도 만든 아이템 — 최초 제작 보너스(F10-04 +25%) 판정. 부활해도 남는다 (PlayerState). */
	TSet<FName> CraftedOnce;

	UFUNCTION()
	void OnRep_Bag();

	virtual void BeginPlay() override;
};
