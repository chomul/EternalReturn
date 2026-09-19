// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FERLootRow;
struct FERItemInstance;

/** 루트 테이블 조회 · 롤 (F09-03). ⚠ 롤은 **서버**에서만 부른다 — 클라가 굴리면 조작된다 (§7.1). */
namespace ERLoot
{
	/** 행 조회. 없으면 nullptr + Error. 항목의 아이템 ID 가 DT_Items 에 없으면 Error (행은 돌려준다 — 그 항목만 롤에서 빠진다). */
	const FERLootRow* Find(FName RowName);

	/** 가중치 무작위로 RollCount 종을 중복 없이 뽑아 Out 에 채운다 (수량은 Min~Max). Weight 0 · 없는 ID 는 제외. */
	void Roll(const FERLootRow& Row, TArray<FERItemInstance>& Out);
}
