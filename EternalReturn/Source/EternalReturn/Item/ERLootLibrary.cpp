// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/ERLootLibrary.h"

#include "Engine/DataTable.h"
#include "EternalReturn.h"
#include "Item/ERInventoryComponent.h"
#include "Item/ERItemData.h"
#include "Item/ERItemSettings.h"
#include "Item/ERLootTypes.h"

namespace
{
	const UDataTable* GetLootTable()
	{
		const UERItemSettings& Settings = UERItemSettings::Get();
		if (Settings.LootTable.IsNull())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[루트] Project Settings > Game > ER Items 에 LootTable 이 비어 있다."));
			return nullptr;
		}
		const UDataTable* Table = Settings.LootTable.LoadSynchronous();
		if (!Table)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[루트] LootTable 을 로드하지 못했다: %s"), *Settings.LootTable.ToString());
			return nullptr;
		}
		if (Table->GetRowStruct() != FERLootRow::StaticStruct())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[루트] %s 의 행 구조가 FERLootRow 가 아니다 (%s)."), *Table->GetName(), *GetNameSafe(Table->GetRowStruct()));
			return nullptr;
		}
		return Table;
	}
}

namespace ERLoot
{

const FERLootRow* Find(FName RowName)
{
	const UDataTable* Table = GetLootTable();
	if (!Table)
	{
		return nullptr;
	}
	const FERLootRow* Row = Table->FindRow<FERLootRow>(RowName, TEXT("ERLoot"), /*bWarnIfRowMissing=*/false);
	if (!Row)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[루트] '%s' 행이 LootTable 에 없다."), *RowName.ToString());
		return nullptr;
	}
	for (const FERLootEntry& E : Row->Entries)
	{
		ERItem::Find(E.ItemId);   // 없으면 Error 로그 — 조용히 빠지지 않게
	}
	return Row;
}

void Roll(const FERLootRow& Row, TArray<FERItemInstance>& Out)
{
	Out.Reset();

	// 후보 = Weight > 0 이고 아이템이 있는 항목
	TArray<const FERLootEntry*> Pool;
	for (const FERLootEntry& E : Row.Entries)
	{
		if (E.Weight > 0 && ERItem::Find(E.ItemId))
		{
			Pool.Add(&E);
		}
	}

	// RollCount 종을 중복 없이 — 뽑은 항목은 풀에서 뺀다 (자체 결정값).
	const int32 Rolls = FMath::Min(Row.RollCount, Pool.Num());
	for (int32 r = 0; r < Rolls; ++r)
	{
		int32 Total = 0;
		for (const FERLootEntry* E : Pool) { Total += E->Weight; }
		int32 Pick = FMath::RandRange(1, Total);
		int32 Chosen = 0;
		for (; Chosen < Pool.Num(); ++Chosen)
		{
			Pick -= Pool[Chosen]->Weight;
			if (Pick <= 0) { break; }
		}
		const FERLootEntry* E = Pool[FMath::Min(Chosen, Pool.Num() - 1)];
		FERItemInstance& Item = Out.AddDefaulted_GetRef();
		Item.ItemId = E->ItemId;
		Item.Count = FMath::RandRange(E->MinCount, FMath::Max(E->MinCount, E->MaxCount));
		Pool.RemoveAt(FMath::Min(Chosen, Pool.Num() - 1));
	}
}

} // namespace ERLoot
