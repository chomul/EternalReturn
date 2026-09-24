// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ERLootTypes.generated.h"

/** 루트 항목 하나. Weight 0 은 안 나온다. */
USTRUCT(BlueprintType)
struct FERLootEntry
{
	GENERATED_BODY()

	/** DT_Items 행 이름. 없으면 ERLoot::Find 가 Error. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	/** 상대 가중치. 0 = 제외. ⚠ Chance > 0 이면 쓰이지 않는다 (가중치 롤 대상이 아니다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 Weight = 1;

	/**
	 * 독립 확률 (0~1). **0 이면 가중치 롤** 대상(상자 · 채집물의 기존 방식), **> 0 이면 RollCount 와 무관하게 이 확률로 따로 굴린다**.
	 * ⭐ F12-02 야생동물 드랍용 — 가죽은 1.0(확정) · 깃털은 확률 (역기획서 몬스터 §4).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 0.f;

	/**
	 * 떨군 개체의 레벨이 이 값 이상일 때만 후보가 된다. 0 = 조건 없음 (상자 · 채집물은 레벨 0 으로 굴린다).
	 * ⭐ F12-06 [확인] 나무위키: 곰 특수 재료는 **15레벨 이상** · 늑대 생명의 나무 · 운석은 **10레벨 이상**.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MinLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

/**
 * 루트 테이블 한 행 (F09-03). RowName = 상자 · 채집물 종류 ("Box_Basic" · "Gather_Branch" …).
 * ⭐ 지역별 종류 차이는 **행이 다른 것**으로 — 코드에 지역 분기 없음 (배치는 F13). F12 야생동물 드랍(가죽 확정 · 깃털 확률)도 같은 형식.
 * ⭐ 원작 확인 (사용자 2026-09-19): 상자는 **한 번 가져가면 끝** · 채집물(돌 · 꽃 · 나뭇가지)은 **무한**이지만 **한 번에 한 명**만 캔다. 재충전 없음.
 * ⚠ 확률 · 수량 · RollCount · 채집 시간은 자체 결정값 — 원작 (미확인) (장비 역기획서 §4.1).
 */
USTRUCT(BlueprintType)
struct FERLootRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FERLootEntry> Entries;

	/** 한 번에 몇 **종**을 뽑나 (중복 없이 — 같은 항목이 두 번 안 나온다). Entries 보다 크면 전부. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 RollCount = 1;

	/**
	 * 가중치 뽑기(RollCount) **자체를 할 확률** (0~1). 기본 1 = 항상 (상자 · 채집물 — 기존 동작 그대로).
	 * ⭐ F12-06: "3.13% 확률로 일반 재료 중 하나" (멧돼지) = 일반 재료 풀 · RollCount 1 · **RollChance 0.0313**. 확률 항목(Chance > 0)은 이 값과 무관하게 따로 굴린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float RollChance = 1.f;

	/** 채집물 — 가져가도 줄지 않고 사라지지 않는다 (원작 확인: 채집물은 개수 제한 없음). 상자 · 시체는 false. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInfinite = false;

	/** 채집물만. 한 번 캐는 데 걸리는 초 — 이 동안 **다른 사람은 못 캔다** (원작 확인: 한 번에 한 명). 0 = 즉시. 원작 **2초 [확인]** (사용자 2026-09-19). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", EditCondition = "bInfinite"))
	float GatherSeconds = 0.f;
};
