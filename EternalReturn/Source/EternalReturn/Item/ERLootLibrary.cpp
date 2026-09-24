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

void Roll(const FERLootRow& Row, TArray<FERItemInstance>& Out, int32 SourceLevel)
{
	Out.Reset();

	// ⭐ 확률 항목(Chance > 0)은 RollCount 와 무관하게 **따로** 굴린다 — 가죽 확정(1.0) · 깃털 확률 (F12-02 · 역기획서 몬스터 §4).
	//   가중치 롤과 섞지 않는다: 섞으면 "확정"이 다른 항목에 밀려 안 나올 수 있다.
	TArray<const FERLootEntry*> Pool;
	for (const FERLootEntry& E : Row.Entries)
	{
		if (!ERItem::Find(E.ItemId) || SourceLevel < E.MinLevel)
		{
			continue;   // 없는 아이템 · 레벨 조건 미달 (곰 특수 재료 15레벨 등 — F12-06)
		}
		if (E.Chance > 0.f)
		{
			// ⚠ Chance 1 은 굴리지 않는다 — FMath::FRand() 는 정확히 1.0 을 돌려줄 때가 있어 "확정" 이 드물게 빠졌다
			//   (2026-09-24 ER.Loot.Sim 곰 1000번: 가죽 999번).
			if (E.Chance >= 1.f || FMath::FRand() < E.Chance)
			{
				FERItemInstance& Item = Out.AddDefaulted_GetRef();
				Item.ItemId = E.ItemId;
				Item.Count = FMath::RandRange(E.MinCount, FMath::Max(E.MinCount, E.MaxCount));
			}
			continue;
		}
		if (E.Weight > 0)
		{
			Pool.Add(&E);
		}
	}

	// 뽑기 자체를 할지 — "p 확률로 풀에서 하나" (F12-06). 기본 1 이면 항상.
	if (Row.RollChance < 1.f && FMath::FRand() >= Row.RollChance)
	{
		return;
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
