// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/ERInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EternalReturn.h"
#include "Character/ERCharacterBase.h"
#include "Character/ERCharacterData.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Item/EREquipmentEffect.h"
#include "Item/ERItemData.h"
#include "Item/ERItemDropActor.h"
#include "Item/ERItemSettings.h"
#include "Net/UnrealNetwork.h"

namespace
{
	int32 GNextDropId = 1;   // 서버 프로세스 안에서만 의미 있는 번호. 로그 · 디버그용

	/** 시체에 남는가 — 원작 확인: 재료 전부 · 영웅(보라) 장비. 전설 · 초월 장비는 안 남는다. 영웅 미만 장비 포함은 자체 결정값. */
	bool DropsOnDeath(const FERItemRow& Item)
	{
		if (!Item.IsEquipment())
		{
			return true;
		}
		return Item.Grade <= EERItemGrade::Hero;
	}
}

UERInventoryComponent::UERInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UERInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UERInventoryComponent, Equipped);
	// ⭐ 가방은 소유자에게만. 24명분이 전원에게 가면 정보 유출 + 트래픽 (Argument 21).
	DOREPLIFETIME_CONDITION(UERInventoryComponent, Bag, COND_OwnerOnly);
}

void UERInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	// 칸 수는 설정값 — 하드코딩 없음. 서버가 채우고 복제로 간다.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Bag.SetNum(FMath::Max(UERItemSettings::Get().InventorySlots, 1));
	}
}

void UERInventoryComponent::OnRep_Bag()
{
	// UI 가 생기면 여기서 갱신. 지금은 로그만.
	UE_LOG(LogEternalReturn, Verbose, TEXT("[인벤토리] %s 가방 복제 (%d칸)"), *GetNameSafe(GetOwner()), Bag.Num());
}

// ─────────────────────────────────────────────────────────────
// 가방
// ─────────────────────────────────────────────────────────────

bool UERInventoryComponent::HasSpaceFor(FName ItemId, int32 Count) const
{
	const FERItemRow* Item = ERItem::Find(ItemId);
	if (!Item || Count <= 0)
	{
		return false;
	}
	int32 Room = 0;
	for (const FERItemInstance& Slot : Bag)
	{
		if (Slot.IsEmpty())                 { Room += Item->MaxStack; }
		else if (Slot.ItemId == ItemId)     { Room += FMath::Max(Item->MaxStack - Slot.Count, 0); }
		if (Room >= Count)
		{
			return true;
		}
	}
	return false;
}

int32 UERInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[인벤토리] AddItem 은 서버에서만 부른다."));
		return Count;
	}
	const FERItemRow* Item = ERItem::Find(ItemId);
	if (!Item || Count <= 0)
	{
		return Count;
	}

	int32 Remaining = Count;

	// ① 같은 아이템 스택부터 (MaxStack 은 아이템마다 다르다 — 원작 확인)
	for (FERItemInstance& Slot : Bag)
	{
		if (Remaining <= 0) { break; }
		if (!Slot.IsEmpty() && Slot.ItemId == ItemId && Slot.Count < Item->MaxStack)
		{
			const int32 Take = FMath::Min(Item->MaxStack - Slot.Count, Remaining);
			Slot.Count += Take;
			Remaining -= Take;
		}
	}
	// ② 빈 칸
	for (FERItemInstance& Slot : Bag)
	{
		if (Remaining <= 0) { break; }
		if (Slot.IsEmpty())
		{
			const int32 Take = FMath::Min(Item->MaxStack, Remaining);
			Slot.ItemId = ItemId;
			Slot.Count = Take;
			Remaining -= Take;
		}
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[인벤토리] %s <- %s ×%d (%s%d 남음)"),
		*GetNameSafe(GetOwner()), *ItemId.ToString(), Count - Remaining, Remaining > 0 ? TEXT("⚠ 가득 참, ") : TEXT(""), Remaining);
	return Remaining;
}

bool UERInventoryComponent::RemoveItem(int32 BagIndex, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[인벤토리] RemoveItem 은 서버에서만 부른다."));
		return false;
	}
	if (!Bag.IsValidIndex(BagIndex) || Bag[BagIndex].IsEmpty() || Bag[BagIndex].Count < Count || Count <= 0)
	{
		return false;
	}
	FERItemInstance& Slot = Bag[BagIndex];
	const FName Id = Slot.ItemId;
	Slot.Count -= Count;
	if (Slot.Count <= 0)
	{
		Slot = FERItemInstance();
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[인벤토리] %s -> %s ×%d 제거 (칸 %d)"), *GetNameSafe(GetOwner()), *Id.ToString(), Count, BagIndex);
	return true;
}

UAbilitySystemComponent* UERInventoryComponent::GetASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

FName UERInventoryComponent::GetEquippedItem(EEREquipSlot Slot) const
{
	const FEREquippedSlot* Found = FindEquippedSlot(Slot);
	return Found ? Found->ItemId : NAME_None;
}

const FEREquippedSlot* UERInventoryComponent::FindEquippedSlot(EEREquipSlot Slot) const
{
	return Equipped.FindByPredicate([Slot](const FEREquippedSlot& E) { return E.Slot == Slot; });
}

// ─────────────────────────────────────────────────────────────
// 드롭 · 습득
// ─────────────────────────────────────────────────────────────

AERItemDropActor* UERInventoryComponent::SpawnDeathDrop(const FVector& Location)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return nullptr;
	}

	// ⭐ 복사 — 원본은 그대로. 가방 + 장착 중.
	TArray<FERItemInstance> Copies;
	int32 Excluded = 0;
	auto Consider = [&](FName ItemId, int32 Count)
	{
		const FERItemRow* Row = ERItem::Find(ItemId);
		if (!Row) { return; }
		if (DropsOnDeath(*Row))
		{
			FERItemInstance Copy; Copy.ItemId = ItemId; Copy.Count = Count;
			Copies.Add(Copy);
		}
		else
		{
			++Excluded;
		}
	};
	for (const FERItemInstance& Slot : Bag)      { if (!Slot.IsEmpty()) { Consider(Slot.ItemId, Slot.Count); } }
	for (const FEREquippedSlot& E : Equipped)   { Consider(E.ItemId, 1); }

	if (Copies.IsEmpty())
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[드롭] %s — 남길 아이템이 없다 (제외 %d개)."), *GetNameSafe(GetOwner()), Excluded);
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AERItemDropActor* Drop = GetWorld()->SpawnActor<AERItemDropActor>(AERItemDropActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!Drop)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[드롭] 시체 액터를 스폰하지 못했다."));
		return nullptr;
	}
	Drop->Initialize(GNextDropId++, Copies);

	FString List;
	for (const FERItemInstance& C : Copies) { List += FString::Printf(TEXT(" %s×%d"), *C.ItemId.ToString(), C.Count); }
	UE_LOG(LogEternalReturn, Log, TEXT("[드롭] %s 시체 #%d 생성:%s (전설·초월 제외 %d개) — 원본 인벤토리 유지"),
		*GetNameSafe(GetOwner()), Drop->GetDropId(), *List, Excluded);
	return Drop;
}

bool UERInventoryComponent::ServerPickup_Validate(AERItemDropActor* Drop, int32 Index)
{
	return Index >= 0;   // 액터 null · 거리 · 빈 칸은 정상 실패(로그). Validate 로 끊으면 연결이 닫힌다
}

void UERInventoryComponent::ServerPickup_Implementation(AERItemDropActor* Drop, int32 Index)
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Drop || !IsValid(Drop) || !Pawn)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[습득] %s — 시체가 없거나 이미 사라졌다."), *GetNameSafe(GetOwner()));
		return;
	}

	// ① 거리 — 서버가 잰다. 클라 위치 주장 없음.
	const float RangeUU = UERItemSettings::Get().PickupRange * 100.f;
	const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), Drop->GetActorLocation());
	if (Dist > RangeUU)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[습득] %s — 시체 #%d 까지 %.0fcm > %.0fcm. 거리 밖."),
			*GetNameSafe(GetOwner()), Drop->GetDropId(), Dist, RangeUU);
		return;
	}

	// ② 칸에 아직 있는가 — ⭐ 경쟁의 판정 지점. 먼저 처리된 요청이 가져갔으면 여기서 거절된다.
	if (!Drop->GetItems().IsValidIndex(Index) || Drop->GetItems()[Index].IsEmpty())
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[습득] %s — 시체 #%d 칸 %d 는 이미 없다 (다른 사람이 먼저)."),
			*GetNameSafe(GetOwner()), Drop->GetDropId(), Index);
		return;
	}

	// ③ 여유 — 들어가는 만큼만. 나머지는 시체에 남는다.
	const FERItemInstance Wanted = Drop->GetItems()[Index];
	int32 Fit = 0;
	for (int32 N = Wanted.Count; N > 0; --N)
	{
		if (HasSpaceFor(Wanted.ItemId, N)) { Fit = N; break; }
	}
	if (Fit <= 0)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[습득] %s — 가방이 가득 차 %s 를 못 줍는다."), *GetNameSafe(GetOwner()), *Wanted.ItemId.ToString());
		return;
	}

	const int32 DropId = Drop->GetDropId();
	const int32 Taken = Drop->TakeFromSlot(Index, Fit);   // ⚠ 이 뒤 Drop 은 파괴됐을 수 있다
	const int32 Left = AddItem(Wanted.ItemId, Taken);
	UE_LOG(LogEternalReturn, Log, TEXT("[습득] %s <- 시체 #%d 칸 %d: %s ×%d%s"),
		*GetNameSafe(GetOwner()), DropId, Index, *Wanted.ItemId.ToString(), Taken - Left,
		Left > 0 ? TEXT(" (⚠ 일부 유실)") : TEXT(""));
}

// ─────────────────────────────────────────────────────────────
// RPC
// ─────────────────────────────────────────────────────────────

bool UERInventoryComponent::ServerEquipFromBag_Validate(int32 BagIndex)
{
	return BagIndex >= 0;   // 범위 밖 · 빈 칸은 정상 실패(로그)라 Validate 로 끊지 않는다
}

void UERInventoryComponent::ServerEquipFromBag_Implementation(int32 BagIndex)
{
	EquipFromBag(BagIndex);
}

bool UERInventoryComponent::ServerDiscard_Validate(int32 BagIndex)
{
	return BagIndex >= 0;
}

void UERInventoryComponent::ServerDiscard_Implementation(int32 BagIndex)
{
	// ⚠ 드롭 액터(F08-05) 전까지는 그냥 사라진다.
	if (Bag.IsValidIndex(BagIndex) && !Bag[BagIndex].IsEmpty())
	{
		RemoveItem(BagIndex, Bag[BagIndex].Count);
	}
}

bool UERInventoryComponent::ServerUnequip_Validate(EEREquipSlot Slot)
{
	return Slot != EEREquipSlot::None;
}

void UERInventoryComponent::ServerUnequip_Implementation(EEREquipSlot Slot)
{
	Unequip(Slot);
}

// ─────────────────────────────────────────────────────────────
// 장착 · 해제 (서버)
// ─────────────────────────────────────────────────────────────

bool UERInventoryComponent::EquipDirect(FName ItemId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] EquipDirect 는 서버에서만 부른다."));
		return false;
	}
	const FERItemRow* Item = ERItem::Find(ItemId);   // 없으면 Error 로그
	if (!Item)
	{
		return false;
	}
	// 있던 장비는 버려진다 — 디버그 · 초기 장비 전용
	const int32 Index = Equipped.IndexOfByPredicate([Item](const FEREquippedSlot& E) { return E.Slot == Item->Slot; });
	if (Index != INDEX_NONE)
	{
		if (UAbilitySystemComponent* ASC = GetASC()) { ASC->RemoveActiveGameplayEffect(Equipped[Index].EffectHandle); }
		Equipped.RemoveAt(Index);
	}
	return ApplyEquip(ItemId, *Item);
}

bool UERInventoryComponent::EquipFromBag(int32 BagIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] EquipFromBag 은 서버에서만 부른다."));
		return false;
	}
	if (!Bag.IsValidIndex(BagIndex) || Bag[BagIndex].IsEmpty())
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[장비] 가방 %d칸이 비었거나 없다."), BagIndex);
		return false;
	}
	const FName ItemId = Bag[BagIndex].ItemId;
	const FERItemRow* Item = ERItem::Find(ItemId);
	if (!Item || !Item->IsEquipment())
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[장비] %s 는 장비가 아니다."), *ItemId.ToString());
		return false;
	}

	// 그 슬롯에 있던 장비는 가방으로 돌아간다 — 이 칸이 비면 그 자리를 쓸 수 있으니 순서: 먼저 뺀다.
	// ⚠ 가방이 꽉 찼고 이 칸에 2개 이상이면 돌아올 자리가 없다 → 거부. 아이템이 증발하면 안 된다.
	const FEREquippedSlot* Prev = FindEquippedSlot(Item->Slot);
	const FName PrevId = Prev ? Prev->ItemId : NAME_None;
	const bool bSlotFreesUp = (Bag[BagIndex].Count == 1);
	if (!PrevId.IsNone() && !bSlotFreesUp && !HasSpaceFor(PrevId, 1))
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[장비] 가방이 가득 차 %s 를 돌려놓을 자리가 없다. 장착 거부."), *PrevId.ToString());
		return false;
	}

	if (!RemoveItem(BagIndex, 1))
	{
		return false;
	}
	if (!PrevId.IsNone())
	{
		Unequip(Item->Slot);   // GE 제거 + 가방으로 (자리는 위에서 보장)
	}
	if (!ApplyEquip(ItemId, *Item))
	{
		AddItem(ItemId, 1);    // 실패하면 되돌린다
		return false;
	}
	return true;
}

bool UERInventoryComponent::ApplyEquip(const FName ItemId, const FERItemRow& ItemRow)
{
	const FERItemRow* Item = &ItemRow;

	// ⭐ 서버 판정 — 클라가 무엇을 검사했든 여기서 다시 본다 (F08-02).
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const AERCharacterBase* Character = PS ? Cast<AERCharacterBase>(PS->GetPawn()) : nullptr;
	const UERCharacterData* CharacterData = Character ? Character->GetCharacterData() : nullptr;
	if (!CharacterData)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] %s 의 실험체 데이터가 없다 (폰 없음?)."), *GetNameSafe(GetOwner()));
		return false;
	}
	FString Reason;
	if (!ERItem::CanEquip(*CharacterData, *Item, &Reason))
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[장비] %s 장착 거부 — %s (%s)"), *ItemId.ToString(), *Reason, *GetNameSafe(GetOwner()));
		return false;
	}

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] %s 에 ASC 가 없다."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (FindEquippedSlot(Item->Slot))
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] %s 슬롯이 아직 차 있다 — 부르는 쪽이 먼저 비워야 한다."), *UEnum::GetValueAsString(Item->Slot));
		return false;
	}

	// ── 장비 GE — Infinite 하나, 아이템 값은 SetByCaller ─────
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(GetOwner());
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UEREquipmentEffect::StaticClass(), 1.f, Context);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] 장비 GE 스펙을 만들지 못했다."));
		return false;
	}

	// ⭐ 표의 어트리뷰트 전부에 값을 넣는다 — 아이템에 없는 것은 0. Additive 0 은 무효고, 안 넣으면 GAS 가 경고를 낸다.
	int32 Applied = 0;
	for (const UEREquipmentEffect::FBinding& B : UEREquipmentEffect::GetBindings())
	{
		const float* Value = Item->StatModifiers.Find(B.Attribute);
		Spec->SetSetByCallerMagnitude(B.Tag, Value ? *Value : 0.f);
		Applied += (Value && *Value != 0.f) ? 1 : 0;
	}
	// 표에 없는 어트리뷰트가 행에 있으면 조용히 무시된다 — 드러낸다.
	for (const TPair<FGameplayAttribute, float>& Pair : Item->StatModifiers)
	{
		const bool bBound = UEREquipmentEffect::GetBindings().ContainsByPredicate(
			[&Pair](const UEREquipmentEffect::FBinding& B) { return B.Attribute == Pair.Key; });
		if (!bBound)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[장비] %s 의 %s 는 장비 GE 표(UEREquipmentEffect::GetBindings)에 없어 무시된다."),
				*ItemId.ToString(), *Pair.Key.GetName());
		}
	}

	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec);
	if (!Handle.WasSuccessfullyApplied())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] %s 의 GE 가 적용되지 않았다."), *ItemId.ToString());
		return false;
	}

	FEREquippedSlot NewSlot;
	NewSlot.Slot = Item->Slot;
	NewSlot.ItemId = ItemId;
	NewSlot.EffectHandle = Handle;
	Equipped.Add(NewSlot);

	UE_LOG(LogEternalReturn, Log, TEXT("[장비] %s <- %s 장착 (%s, 스탯 %d개)"),
		*GetNameSafe(GetOwner()), *ItemId.ToString(), *UEnum::GetValueAsString(Item->Slot), Applied);
	OnEquippedChanged.Broadcast(Item->Slot);
	return true;
}

bool UERInventoryComponent::Unequip(EEREquipSlot Slot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[장비] Unequip 은 서버에서만 부른다."));
		return false;
	}

	const int32 Index = Equipped.IndexOfByPredicate([Slot](const FEREquippedSlot& E) { return E.Slot == Slot; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	// 가방으로 돌아갈 자리가 없으면 거부 — 아이템이 증발하면 안 된다 (자체 결정값).
	const FName ItemId = Equipped[Index].ItemId;
	if (!HasSpaceFor(ItemId, 1))
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[장비] 가방이 가득 차 %s 를 벗을 수 없다."), *ItemId.ToString());
		return false;
	}

	// ⭐ 수동 차감 없음 — GE 를 지우면 GAS 가 정확히 원복한다.
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->RemoveActiveGameplayEffect(Equipped[Index].EffectHandle);
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[장비] %s -> %s 해제 (%s) -> 가방"),
		*GetNameSafe(GetOwner()), *ItemId.ToString(), *UEnum::GetValueAsString(Slot));
	Equipped.RemoveAt(Index);
	AddItem(ItemId, 1);
	OnEquippedChanged.Broadcast(Slot);
	return true;
}
