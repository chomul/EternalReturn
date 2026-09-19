// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/ERCraftLibrary.h"

#include "Engine/DataTable.h"
#include "EternalReturn.h"
#include "Item/ERItemData.h"

namespace
{
	/** 정렬된 재료 쌍 — A/B 순서를 지운다. */
	TPair<FName, FName> SortedPair(FName A, FName B)
	{
		return A.LexicalLess(B) ? TPair<FName, FName>(A, B) : TPair<FName, FName>(B, A);
	}

	struct FCraftIndex
	{
		TWeakObjectPtr<const UDataTable> Table;                 // 어느 테이블로 만들었나 — 바뀌면 다시
		TMap<TPair<FName, FName>, FName> Forward;               // 정렬쌍 → 결과
		TMultiMap<FName, FName> UsedIn;                         // 재료 → 결과들
		FDelegateHandle ChangedHandle;

		void Reset()
		{
			if (const UDataTable* T = Table.Get(); T && ChangedHandle.IsValid())
			{
				const_cast<UDataTable*>(T)->OnDataTableChanged().Remove(ChangedHandle);
			}
			Table.Reset();
			Forward.Reset();
			UsedIn.Reset();
			ChangedHandle.Reset();
		}
	};

	FCraftIndex GIndex;

	/** 인덱스를 (필요하면) 만든다. 테이블이 없으면 nullptr — Error 는 ERItem::GetTable 이 냈다. */
	const FCraftIndex* GetIndex()
	{
		const UDataTable* Table = ERItem::GetTable();
		if (!Table)
		{
			return nullptr;
		}
		if (GIndex.Table.Get() == Table)
		{
			return &GIndex;
		}

		GIndex.Reset();
		GIndex.Table = Table;
		// 에디터에서 행을 고치거나 재임포트하면 버린다 — 다음 조회 때 다시 만든다.
		GIndex.ChangedHandle = const_cast<UDataTable*>(Table)->OnDataTableChanged().AddLambda([]() { ERCraft::InvalidateIndex(); });

		int32 Recipes = 0;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const FERItemRow* Row = reinterpret_cast<const FERItemRow*>(Pair.Value);
			if (!Row || !Row->IsCraftable())
			{
				// 한쪽만 채운 행은 실수다 — 드러낸다 (§3.1 ① 항상 2개).
				if (Row && (Row->CraftMaterialA.IsNone() != Row->CraftMaterialB.IsNone()))
				{
					UE_LOG(LogEternalReturn, Error, TEXT("[제작] %s 의 재료가 한쪽만 있다 (A=%s B=%s). 둘 다 채우거나 둘 다 비운다."),
						*Pair.Key.ToString(), *Row->CraftMaterialA.ToString(), *Row->CraftMaterialB.ToString());
				}
				continue;
			}

			// ⚠ 재료 ID 오타는 여기서 잡는다 — 조용히 "제작 불가" 가 되면 못 찾는다.
			for (const FName Material : { Row->CraftMaterialA, Row->CraftMaterialB })
			{
				if (!Table->GetRowMap().Contains(Material))
				{
					UE_LOG(LogEternalReturn, Error, TEXT("[제작] %s 의 재료 '%s' 가 DT_Items 에 없다 (오타?)."), *Pair.Key.ToString(), *Material.ToString());
				}
			}

			const TPair<FName, FName> Key = SortedPair(Row->CraftMaterialA, Row->CraftMaterialB);
			if (const FName* Existing = GIndex.Forward.Find(Key))
			{
				// 같은 조합이 두 결과를 내면 어느 쪽이 맞는지 알 수 없다 — 원작에는 없다 (§3.1 ①).
				UE_LOG(LogEternalReturn, Error, TEXT("[제작] %s + %s 가 %s 와 %s 둘 다를 만든다. 조합은 결과 하나여야 한다."),
					*Key.Key.ToString(), *Key.Value.ToString(), *Existing->ToString(), *Pair.Key.ToString());
				continue;
			}
			GIndex.Forward.Add(Key, Pair.Key);
			GIndex.UsedIn.AddUnique(Row->CraftMaterialA, Pair.Key);
			if (Row->CraftMaterialB != Row->CraftMaterialA)
			{
				GIndex.UsedIn.AddUnique(Row->CraftMaterialB, Pair.Key);
			}
			++Recipes;
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[제작] 인덱스 생성 — 행 %d · 조합 %d"), Table->GetRowMap().Num(), Recipes);
		return &GIndex;
	}

	bool ExpandRecursive(FName ItemId, TMap<FName, int32>& OutLeaves, TArray<FName>& Path)
	{
		if (Path.Contains(ItemId))
		{
			FString Cycle;
			for (const FName& P : Path) { Cycle += P.ToString() + TEXT(" -> "); }
			UE_LOG(LogEternalReturn, Error, TEXT("[제작] 순환 조합: %s%s. 데이터 오류 — 탐색 중단."), *Cycle, *ItemId.ToString());
			return false;
		}

		const FERItemRow* Row = ERItem::Find(ItemId);   // 없으면 Error 로그
		if (!Row)
		{
			return false;
		}
		if (!Row->IsCraftable())
		{
			OutLeaves.FindOrAdd(ItemId) += 1;   // 잎
			return true;
		}

		Path.Push(ItemId);
		const bool bOk = ExpandRecursive(Row->CraftMaterialA, OutLeaves, Path) && ExpandRecursive(Row->CraftMaterialB, OutLeaves, Path);
		Path.Pop();
		return bOk;
	}
}

namespace ERCraft
{

bool GetMaterials(FName ResultId, FName& OutA, FName& OutB)
{
	const FERItemRow* Row = ERItem::Find(ResultId);
	if (!Row || !Row->IsCraftable())
	{
		return false;
	}
	OutA = Row->CraftMaterialA;
	OutB = Row->CraftMaterialB;
	return true;
}

FName FindResult(FName A, FName B)
{
	const FCraftIndex* Index = GetIndex();
	if (!Index || A.IsNone() || B.IsNone())
	{
		return NAME_None;
	}
	const FName* Result = Index->Forward.Find(SortedPair(A, B));
	return Result ? *Result : NAME_None;
}

void FindUsing(FName MaterialId, TArray<FName>& OutResults)
{
	OutResults.Reset();
	if (const FCraftIndex* Index = GetIndex())
	{
		Index->UsedIn.MultiFind(MaterialId, OutResults, /*bMaintainOrder=*/true);
	}
}

bool ExpandLeaves(FName ResultId, TMap<FName, int32>& OutLeaves)
{
	OutLeaves.Reset();
	TArray<FName> Path;
	return ExpandRecursive(ResultId, OutLeaves, Path);
}

void InvalidateIndex()
{
	GIndex.Reset();
}

} // namespace ERCraft
