// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 제작 조회 (F09-01). ⭐ **순수 함수** — DT_Items 만 읽는다. 인벤토리 · 액터를 모르니 서버 · 클라 · UI 가 같은 답을 낸다.
 * 소비 · 검증(누가 만드나)은 F09-02 (UERInventoryComponent::ServerCraft).
 *
 * 인덱스는 테이블 로드 시 **한 번** 만든다 (수백 행을 매번 훑지 않는다). 테이블이 바뀌면(에디터 재임포트) 자동 무효화.
 */
namespace ERCraft
{
	/** 결과 → 재료 2개. 제작 불가(재료 없음)면 false. */
	bool GetMaterials(FName ResultId, FName& OutA, FName& OutB);

	/** 정방향: A + B → 결과. 순서 무관. 없으면 None. */
	FName FindResult(FName A, FName B);

	/** 역방향: 이 재료를 쓰는 결과들 ("이 돌멩이로 뭘 만들 수 있나"). */
	void FindUsing(FName MaterialId, TArray<FName>& OutResults);

	/**
	 * 재귀: ResultId 를 만들려면 **잎 재료**(재료가 없는 아이템)가 무엇이 몇 개 필요한가. 같은 잎이 여러 갈래에서 나오면 합산.
	 * 깊이 제한 없음 — 초월 건너뛰기가 그대로 나온다. 순환(A 가 A 를 요구)이면 Error 로그 + false.
	 * ResultId 자체가 잎이면 {ResultId: 1}.
	 */
	bool ExpandLeaves(FName ResultId, TMap<FName, int32>& OutLeaves);

	/** 인덱스를 버린다 — 테이블 교체 · 디버그. 다음 조회 때 다시 만든다. */
	void InvalidateIndex();
}
