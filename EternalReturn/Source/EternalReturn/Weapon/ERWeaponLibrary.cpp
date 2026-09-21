// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/ERWeaponLibrary.h"

#include "Engine/DataTable.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "Weapon/ERWeaponSettings.h"
#include "Weapon/ERWeaponTypes.h"

namespace
{
	const UDataTable* GetWeaponTable()
	{
		const UERWeaponSettings& Settings = UERWeaponSettings::Get();
		if (Settings.WeaponClassTable.IsNull())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[무기] Project Settings > Game > ER Weapons 에 WeaponClassTable 이 비어 있다."));
			return nullptr;
		}
		const UDataTable* Table = Settings.WeaponClassTable.LoadSynchronous();
		if (!Table)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[무기] WeaponClassTable 을 로드하지 못했다: %s"), *Settings.WeaponClassTable.ToString());
			return nullptr;
		}
		if (Table->GetRowStruct() != FERWeaponClassRow::StaticStruct())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[무기] %s 의 행 구조가 FERWeaponClassRow 가 아니다 (%s)."), *Table->GetName(), *GetNameSafe(Table->GetRowStruct()));
			return nullptr;
		}
		return Table;
	}

	FName RowNameFor(EERWeaponType WeaponType)
	{
		// RowName = enum 값 이름 ("Hammer"). GetValueAsString 은 "EERWeaponType::Hammer" 라 뒤만 쓴다.
		FString S = UEnum::GetValueAsString(WeaponType);
		int32 Sep = INDEX_NONE;
		if (S.FindLastChar(TEXT(':'), Sep)) { S = S.Mid(Sep + 1); }
		return FName(*S);
	}

	/** 행 하나의 스킬 데이터 검사. 틀린 게 있으면 false + Error. */
	bool CheckRow(FName RowName, const FERWeaponClassRow& Row)
	{
		bool bOk = true;
		if (Row.WeaponType == EERWeaponType::None || RowNameFor(Row.WeaponType) != RowName)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[무기] 행 '%s' 의 WeaponType(%s) 이 행 이름과 다르다."), *RowName.ToString(), *UEnum::GetValueAsString(Row.WeaponType));
			bOk = false;
		}
		if (Row.DSkill)
		{
			if (!Row.DSkill->SlotTag.MatchesTagExact(ERTags::Ability_Slot_D))
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[무기] %s 의 DSkill %s 의 SlotTag 가 Ability.Slot.D 가 아니다 (%s)."),
					*RowName.ToString(), *GetNameSafe(Row.DSkill), *Row.DSkill->SlotTag.ToString());
				bOk = false;
			}
			if (Row.DSkill->bUsesSkillPoints || Row.DSkill->InitialLevel != 0)
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[무기] %s 의 DSkill %s 는 bUsesSkillPoints=false · InitialLevel=0 이어야 한다 (숙련도가 올린다)."),
					*RowName.ToString(), *GetNameSafe(Row.DSkill));
				bOk = false;
			}
			if (Row.DSkill->MaxLevel != Row.UpgradeLevels.Num() + 1)
			{
				UE_LOG(LogEternalReturn, Warning, TEXT("[무기] %s 의 DSkill %s MaxLevel(%d) 이 해금 1 + 강화 %d 와 다르다."),
					*RowName.ToString(), *GetNameSafe(Row.DSkill), Row.DSkill->MaxLevel, Row.UpgradeLevels.Num());
			}
		}
		if (Row.AttackData && !Row.AttackData->SlotTag.MatchesTagExact(ERTags::Ability_Slot_Attack))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[무기] %s 의 AttackData %s 의 SlotTag 가 Ability.Slot.Attack 이 아니다 (%s)."),
				*RowName.ToString(), *GetNameSafe(Row.AttackData), *Row.AttackData->SlotTag.ToString());
			bOk = false;
		}
		return bOk;
	}
}

namespace ERWeapon
{

const FERWeaponClassRow* Find(EERWeaponType WeaponType)
{
	const UDataTable* Table = GetWeaponTable();
	if (!Table || WeaponType == EERWeaponType::None)
	{
		return nullptr;
	}
	const FName RowName = RowNameFor(WeaponType);
	const FERWeaponClassRow* Row = Table->FindRow<FERWeaponClassRow>(RowName, TEXT("ERWeapon"), /*bWarnIfRowMissing=*/false);
	if (!Row)
	{
		// ⚠ 23종 중 행이 없는 계열 — 조용히 "D 없음" 이 되지 않게 드러낸다 (초기 8행 밖의 계열을 장착했다).
		UE_LOG(LogEternalReturn, Error, TEXT("[무기] 계열 '%s' 행이 WeaponClassTable 에 없다. 이 계열은 D · 사거리 없이 동작한다."), *RowName.ToString());
		return nullptr;
	}
	CheckRow(RowName, *Row);
	return Row;
}

bool ValidateTable()
{
	const UDataTable* Table = GetWeaponTable();
	if (!Table)
	{
		return false;
	}
	bool bOk = true;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FERWeaponClassRow* Row = reinterpret_cast<const FERWeaponClassRow*>(Pair.Value);
		if (Row && !CheckRow(Pair.Key, *Row))
		{
			bOk = false;
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[무기] WeaponClassTable 검사 — 행 %d · %s"), Table->GetRowMap().Num(), bOk ? TEXT("이상 없음") : TEXT("오류 있음 (위 Error)"));
	return bOk;
}

} // namespace ERWeapon
