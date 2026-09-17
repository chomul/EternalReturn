// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ **임시 파일이다. 인벤토리 UI(F17)가 생기면 통째로 삭제한다.**
//   아이템 테이블을 눈으로 확인할 수단이 아직 없다. 그때까지 콘솔로 대신한다.

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EternalReturn.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/ERCharacterBase.h"
#include "Character/ERCharacterData.h"
#include "Core/ERPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ERAttributeSet.h"
#include "Item/ERInventoryComponent.h"
#include "Item/ERItemData.h"
#include "Item/ERItemDropActor.h"
#include "EngineUtils.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillDamageEffect.h"

namespace
{

FString DescribeRow(FName Id, const FERItemRow& Row)
{
	FString Stats;
	for (const TPair<FGameplayAttribute, float>& Pair : Row.StatModifiers)
	{
		Stats += FString::Printf(TEXT(" %s%+.2f"), *Pair.Key.GetName(), Pair.Value);
	}
	return FString::Printf(TEXT("%s | %s | %s | %s | %s |%s | 아이콘 %s(%s)"),
		*Id.ToString(), *Row.DisplayName.ToString(),
		*UEnum::GetValueAsString(Row.Grade), *UEnum::GetValueAsString(Row.Slot), *UEnum::GetValueAsString(Row.WeaponType),
		Stats.IsEmpty() ? TEXT(" (스탯 없음)") : *Stats,
		Row.Icon.IsNull() ? TEXT("없음") : *Row.Icon.ToSoftObjectPath().GetAssetName(),
		// ⭐ 로드 여부 — 테이블을 읽었다고 아이콘이 로드되면 안 된다 (체크리스트 "아이콘을 참조만 하고 로드하지 않는다")
		Row.Icon.IsNull() ? TEXT("-") : Row.Icon.IsValid() ? TEXT("⚠ 로드됨") : TEXT("미로드"));
}

// ER.Item.Show <ID>
void ItemShowCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] 사용법: ER.Item.Show <행 이름>"));
		return;
	}
	const FName Id(*Args[0]);
	if (const FERItemRow* Row = ERItem::Find(Id))
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] %s"), *DescribeRow(Id, *Row));
	}
}

// ER.Item.List — 전체 순회 (F09 제작 트리가 쓸 경로와 같다)
void ItemListCmd(const TArray<FString>& Args, UWorld* World)
{
	const UDataTable* Table = ERItem::GetTable();
	if (!Table)
	{
		return;
	}
	int32 Count = 0;
	Table->ForeachRow<FERItemRow>(TEXT("ER.Item.List"), [&Count](const FName& Id, const FERItemRow& Row)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] %s"), *DescribeRow(Id, Row));
		++Count;
	});
	UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] %s — %d행"), *Table->GetName(), Count);
}

// ER.Item.CanEquip <ID> — 로컬 폰의 실험체 데이터로 장착 가능 판정 (F08-02)
void ItemCanEquipCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] 사용법: ER.Item.CanEquip <행 이름>"));
		return;
	}
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const AERCharacterBase* Character = PC ? Cast<AERCharacterBase>(PC->GetPawn()) : nullptr;
	const UERCharacterData* Data = Character ? Character->GetCharacterData() : nullptr;
	if (!Data)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템디버그] 조종 중인 폰의 실험체 데이터가 없다."));
		return;
	}
	const FName Id(*Args[0]);
	const FERItemRow* Row = ERItem::Find(Id);
	if (!Row)
	{
		return;
	}
	FString Reason;
	const bool bOk = ERItem::CanEquip(*Data, *Row, &Reason);
	UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] %s 장착 %s%s  (실험체 %s 무기군 %d개)"),
		*Id.ToString(), bOk ? TEXT("가능") : TEXT("불가 — "), bOk ? TEXT("") : *Reason,
		*GetNameSafe(Data), Data->WeaponTypes.Num());
}

UERInventoryComponent* LocalInventory(UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const AERPlayerState* PS = PC ? PC->GetPlayerState<AERPlayerState>() : nullptr;
	UERInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Inv)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템디버그] 인벤토리 컴포넌트를 찾지 못했다."));
	}
	return Inv;
}

bool ParseSlot(const FString& Name, EEREquipSlot& Out)
{
	static const TMap<FString, EEREquipSlot> Table = {
		{ TEXT("Weapon"), EEREquipSlot::Weapon }, { TEXT("Head"), EEREquipSlot::Head }, { TEXT("Chest"), EEREquipSlot::Chest },
		{ TEXT("Arm"), EEREquipSlot::Arm }, { TEXT("Leg"), EEREquipSlot::Leg },
	};
	for (const TPair<FString, EEREquipSlot>& P : Table)
	{
		if (P.Key.Equals(Name, ESearchCase::IgnoreCase)) { Out = P.Value; return true; }
	}
	return false;
}

// ER.Item.Give <ID> [n] — 서버 창에서만. 습득(F08-05) 전까지 가방에 직접 넣는다.
//   ⚠ 클라에서 치면 거부 — "클라가 직접 추가 못 한다" 의 증거.
void ItemGiveCmd(const TArray<FString>& Args, UWorld* World)
{
	UERInventoryComponent* Inv = LocalInventory(World);
	if (!Inv || Args.Num() < 1)
	{
		return;
	}
	if (!Inv->GetOwner()->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템디버그] 서버 창에서만 된다 — 클라는 아이템을 직접 넣을 수 없다."));
		return;
	}
	const int32 Count = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 1;
	Inv->AddItem(FName(*Args[0]), Count);
}

// ER.Item.Equip <ID> — 서버 창에서만. 가방을 거치지 않는 직접 장착 (디버그)
void ItemEquipCmd(const TArray<FString>& Args, UWorld* World)
{
	UERInventoryComponent* Inv = LocalInventory(World);
	if (!Inv || Args.Num() < 1)
	{
		return;
	}
	if (!Inv->GetOwner()->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템디버그] 서버 창에서만 된다. 클라는 ER.Item.EquipSlot <가방칸> 으로."));
		return;
	}
	Inv->EquipDirect(FName(*Args[0]));
}

// ER.Item.EquipSlot <가방칸> — 어느 창에서든 (실제 게임 경로: RPC)
void ItemEquipSlotCmd(const TArray<FString>& Args, UWorld* World)
{
	UERInventoryComponent* Inv = LocalInventory(World);
	if (!Inv || Args.Num() < 1)
	{
		return;
	}
	Inv->ServerEquipFromBag(FCString::Atoi(*Args[0]));
}

// ER.Item.Discard <가방칸>
void ItemDiscardCmd(const TArray<FString>& Args, UWorld* World)
{
	UERInventoryComponent* Inv = LocalInventory(World);
	if (!Inv || Args.Num() < 1)
	{
		return;
	}
	Inv->ServerDiscard(FCString::Atoi(*Args[0]));
}

// ER.Item.Bag — 이 월드가 아는 모든 플레이어의 가방. ⭐ 클라에서는 남의 가방이 **비어** 보여야 한다 (COND_OwnerOnly)
void ItemBagCmd(const TArray<FString>& Args, UWorld* World)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	for (APlayerState* PS : GS->PlayerArray)
	{
		const AERPlayerState* ERPS = Cast<AERPlayerState>(PS);
		const UERInventoryComponent* Inv = ERPS ? ERPS->GetInventory() : nullptr;
		if (!Inv)
		{
			continue;
		}
		FString Line;
		int32 Used = 0;
		for (int32 i = 0; i < Inv->GetBag().Num(); ++i)
		{
			const FERItemInstance& Slot = Inv->GetBag()[i];
			if (!Slot.IsEmpty())
			{
				Line += FString::Printf(TEXT(" [%d]%s×%d"), i, *Slot.ItemId.ToString(), Slot.Count);
				++Used;
			}
		}
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그][%s] %s 가방 %d/%d칸:%s"),
			Side, *PS->GetName(), Used, Inv->GetBag().Num(),
			Inv->GetBag().Num() == 0 ? TEXT(" (복제 안 됨 — 남의 가방)") : Line.IsEmpty() ? TEXT(" 비어 있음") : *Line);
	}
}

// ER.Item.Unequip <Weapon|Head|Chest|Arm|Leg>
void ItemUnequipCmd(const TArray<FString>& Args, UWorld* World)
{
	UERInventoryComponent* Inv = LocalInventory(World);
	EEREquipSlot Slot;
	if (!Inv || Args.Num() < 1 || !ParseSlot(Args[0], Slot))
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] 사용법: ER.Item.Unequip <Weapon|Head|Chest|Arm|Leg>"));
		return;
	}
	Inv->ServerUnequip(Slot);
}

// ER.Item.Equipped — 이 월드가 아는 모든 플레이어의 장착 칸 (복제 확인)
void ItemEquippedCmd(const TArray<FString>& Args, UWorld* World)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	static const EEREquipSlot Slots[] = { EEREquipSlot::Weapon, EEREquipSlot::Head, EEREquipSlot::Chest, EEREquipSlot::Arm, EEREquipSlot::Leg };
	for (APlayerState* PS : GS->PlayerArray)
	{
		const AERPlayerState* ERPS = Cast<AERPlayerState>(PS);
		const UERInventoryComponent* Inv = ERPS ? ERPS->GetInventory() : nullptr;
		if (!Inv)
		{
			continue;
		}
		FString Line;
		for (EEREquipSlot S : Slots)
		{
			const FName Id = Inv->GetEquippedItem(S);
			if (!Id.IsNone())
			{
				Line += FString::Printf(TEXT(" %s=%s"), *UEnum::GetValueAsString(S), *Id.ToString());
				// 서버면 GE 가 Infinite 인지도 찍는다 (체크리스트 "GE 종류가 Infinite 임을 실제로 확인")
				if (World->GetNetMode() != NM_Client)
				{
					const FEREquippedSlot* E = Inv->FindEquippedSlot(S);
					const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
					const FActiveGameplayEffect* GE = (E && ASC) ? ASC->GetActiveGameplayEffect(E->EffectHandle) : nullptr;
					Line += GE ? (GE->GetDuration() == UGameplayEffect::INFINITE_DURATION ? TEXT("(Infinite)") : TEXT("(⚠ Infinite 아님)")) : TEXT("(⚠ GE 없음)");
				}
			}
		}
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그][%s] %s 장착:%s"), Side, *PS->GetName(), Line.IsEmpty() ? TEXT(" 없음") : *Line);
	}
}

// ER.Item.Stats — 로컬 폰의 주요 어트리뷰트 Base / Current / Bonus(= Current − Base, 추가 공격력 정의)
void ItemStatsCmd(const TArray<FString>& Args, UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const UAbilitySystemComponent* ASC = PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
	if (!ASC)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템디버그] ASC 를 찾지 못했다."));
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	const FGameplayAttribute Attrs[] = {
		UERAttributeSet::GetAttackPowerAttribute(), UERAttributeSet::GetDefenseAttribute(),
		UERAttributeSet::GetAttackRangeAttribute(), UERAttributeSet::GetMaxHPAttribute(), UERAttributeSet::GetAttackSpeedAttribute(),
	};
	FString Line;
	for (const FGameplayAttribute& A : Attrs)
	{
		const float Base = ASC->GetNumericAttributeBase(A);
		const float Cur = ASC->GetNumericAttribute(A);
		Line += FString::Printf(TEXT("  %s: 기본 %.2f / 현재 %.2f / 추가 %+.2f"), *A.GetName(), Base, Cur, Cur - Base);
	}
	UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그][%s] %s"), Side, *Line);
}

// ER.Item.Die — 서버 창. 자기에게 고정 피해 99999 (실제 피해 경로 → OnOutOfHealth → 시체)
void ItemDieCmd(const TArray<FString>& Args, UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UAbilitySystemComponent* ASC = PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템디버그] 서버 창에서만 된다."));
		return;
	}
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UERSkillDamageEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(ERTags::Data_Damage_Base, 99999.f);
		Spec.Data->AddDynamicAssetTag(ERTags::Damage_Type_True);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
}

// ER.Item.Drops — 이 월드의 시체 전부 (클라도 복제된 만큼 보인다)
void ItemDropsCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World) { return; }
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	int32 Count = 0;
	for (TActorIterator<AERItemDropActor> It(World); It; ++It)
	{
		FString Line;
		for (int32 i = 0; i < It->GetItems().Num(); ++i)
		{
			const FERItemInstance& I = It->GetItems()[i];
			if (!I.IsEmpty()) { Line += FString::Printf(TEXT(" [%d]%s×%d"), i, *I.ItemId.ToString(), I.Count); }
		}
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그][%s] 시체 #%d @%s:%s"), Side, It->GetDropId(),
			*It->GetActorLocation().ToCompactString(), Line.IsEmpty() ? TEXT(" 비어 있음") : *Line);
		++Count;
	}
	if (Count == 0) { UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그][%s] 시체 없음"), Side); }
}

// ER.Item.Pickup <DropId> <칸> — 어느 창에서든 (실제 경로: RPC)
void ItemPickupCmd(const TArray<FString>& Args, UWorld* World)
{
	UERInventoryComponent* Inv = LocalInventory(World);
	if (!Inv || Args.Num() < 2)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[아이템디버그] 사용법: ER.Item.Pickup <DropId> <칸>"));
		return;
	}
	const int32 DropId = FCString::Atoi(*Args[0]);
	AERItemDropActor* Found = nullptr;
	for (TActorIterator<AERItemDropActor> It(World); It; ++It)
	{
		if (It->GetDropId() == DropId) { Found = *It; break; }
	}
	if (!Found)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템디버그] 시체 #%d 를 이 월드에서 못 찾았다 (아직 복제 안 됐거나 사라짐)."), DropId);
		return;
	}
	Inv->ServerPickup(Found, FCString::Atoi(*Args[1]));
}

} // namespace

static FAutoConsoleCommandWithWorldAndArgs GERItemDieCmd(
	TEXT("ER.Item.Die"), TEXT("[임시] 자기에게 고정 피해 99999 (서버 창). 시체 드롭 확인용"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemDieCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemDropsCmd(
	TEXT("ER.Item.Drops"), TEXT("[임시] 월드의 시체 목록"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemDropsCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemPickupCmd(
	TEXT("ER.Item.Pickup"), TEXT("[임시] 습득 요청 (클라 -> 서버). ER.Item.Pickup <DropId> <칸>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemPickupCmd));

static FAutoConsoleCommandWithWorldAndArgs GERItemGiveCmd(
	TEXT("ER.Item.Give"), TEXT("[임시] 가방에 넣기 (서버 창). ER.Item.Give <ID> [n]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemGiveCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemEquipCmd(
	TEXT("ER.Item.Equip"), TEXT("[임시] 직접 장착 (서버 창, 가방 안 거침). ER.Item.Equip <ID>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemEquipCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemEquipSlotCmd(
	TEXT("ER.Item.EquipSlot"), TEXT("[임시] 가방 칸 장착 (클라 -> 서버). ER.Item.EquipSlot <칸>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemEquipSlotCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemDiscardCmd(
	TEXT("ER.Item.Discard"), TEXT("[임시] 가방 칸 버리기. ER.Item.Discard <칸>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemDiscardCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemBagCmd(
	TEXT("ER.Item.Bag"), TEXT("[임시] 모든 플레이어의 가방 (클라에선 남의 것은 비어 보임)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemBagCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemUnequipCmd(
	TEXT("ER.Item.Unequip"), TEXT("[임시] 해제 요청. ER.Item.Unequip <Weapon|Head|Chest|Arm|Leg>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemUnequipCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemEquippedCmd(
	TEXT("ER.Item.Equipped"), TEXT("[임시] 모든 플레이어의 장착 칸"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemEquippedCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemStatsCmd(
	TEXT("ER.Item.Stats"), TEXT("[임시] 주요 어트리뷰트 기본/현재/추가"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemStatsCmd));

static FAutoConsoleCommandWithWorldAndArgs GERItemCanEquipCmd(
	TEXT("ER.Item.CanEquip"), TEXT("[임시] 장착 가능 판정. ER.Item.CanEquip <ID>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemCanEquipCmd));

static FAutoConsoleCommandWithWorldAndArgs GERItemShowCmd(
	TEXT("ER.Item.Show"), TEXT("[임시] 아이템 행 하나. ER.Item.Show <ID>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemShowCmd));
static FAutoConsoleCommandWithWorldAndArgs GERItemListCmd(
	TEXT("ER.Item.List"), TEXT("[임시] 아이템 테이블 전체"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ItemListCmd));
