// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/ERItemData.h"

#include "EternalReturn.h"
#include "Character/ERCharacterData.h"
#include "Item/ERItemSettings.h"

namespace ERItem
{

const UDataTable* GetTable()
{
	const UERItemSettings& Settings = UERItemSettings::Get();
	if (Settings.ItemTable.IsNull())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템] Project Settings > Game > ER Items 에 ItemTable 이 비어 있다."));
		return nullptr;
	}

	// 동기 로드 — 테이블 하나뿐이고 아이콘은 소프트 참조라 가볍다. 첫 호출 이후엔 로드된 객체를 돌려준다.
	const UDataTable* Table = Settings.ItemTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템] ItemTable 을 로드하지 못했다: %s"), *Settings.ItemTable.ToString());
		return nullptr;
	}

	if (Table->GetRowStruct() != FERItemRow::StaticStruct())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템] %s 의 행 구조가 FERItemRow 가 아니다 (%s)."),
			*Table->GetName(), *GetNameSafe(Table->GetRowStruct()));
		return nullptr;
	}
	return Table;
}

const FERItemRow* Find(FName ItemId)
{
	const UDataTable* Table = GetTable();
	if (!Table)
	{
		return nullptr;
	}

	const FERItemRow* Row = Table->FindRow<FERItemRow>(ItemId, TEXT("ERItem::Find"), /*bWarnIfRowMissing=*/false);
	if (!Row)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[아이템] '%s' 행이 %s 에 없다. 행 이름(ID) 오타이거나 아직 안 만든 아이템이다."),
			*ItemId.ToString(), *Table->GetName());
	}
	return Row;
}

bool CanEquip(const UERCharacterData& Character, const FERItemRow& Item, FString* OutReason)
{
	if (!Item.IsEquipment())
	{
		if (OutReason) { *OutReason = TEXT("장비가 아니다 (재료 · 소모품)"); }
		return false;
	}

	// ⭐ 무기만 제한이 있다. 방어구 4종은 누구나 (장비 역기획서 §2).
	if (Item.Slot == EEREquipSlot::Weapon)
	{
		if (Item.WeaponType == EERWeaponType::None)
		{
			// 데이터 실수 — 무기 슬롯인데 무기군이 없다. 조용히 통과시키면 누구나 드는 무기가 된다.
			if (OutReason) { *OutReason = TEXT("무기 행에 WeaponType 이 None 이다 (테이블 실수)"); }
			return false;
		}
		if (!Character.WeaponTypes.Contains(Item.WeaponType))
		{
			if (OutReason)
			{
				*OutReason = FString::Printf(TEXT("무기군 %s 를 이 실험체는 못 든다"), *UEnum::GetValueAsString(Item.WeaponType));
			}
			return false;
		}
	}

	return true;
}

} // namespace ERItem
