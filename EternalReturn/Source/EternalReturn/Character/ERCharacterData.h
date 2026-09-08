// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GAS/ERAttributeTypes.h"

#include "ERCharacterData.generated.h"

/**
 * 실험체 하나를 정의하는 데이터 애셋.
 *
 * ⭐ **실험체 하나당 애셋 하나다.** 데이터 테이블에 전부 몰아넣지 않는다.
 *   이유 세 가지 - 근거: Docs/4_Argument/4_스탯데이터_저장방식.md
 *     1. 이 데이터는 커진다 (메시·애님BP·어빌리티·아이콘·음성이 결국 여기 붙는다)
 *     2. 여러 명이 작업할 때 애셋 하나는 병목이다. 캐릭터별이면 각자 자기 파일만 만진다
 *     3. UPrimaryDataAsset 이라 AssetManager 가 비동기 로드·언로드 단위로 다룰 수 있다
 *
 * ⚠ **지금은 스탯만 들어 있다.** 메시·어빌리티는 그 기능을 만들 때 여기 붙인다.
 *   미리 빈 칸을 만들어두지 않는다 (CLAUDE.md §2).
 *
 * ⚠ 밸런싱할 때 여러 실험체를 한눈에 비교하려면 애셋을 여러 개 선택하고
 *   우클릭 > Bulk Edit via Property Matrix 를 쓴다.
 */
UCLASS(BlueprintType, Const, Meta = (DisplayName = "ER 실험체 데이터"))
class ETERNALRETURN_API UERCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 1레벨 기본 스탯.
	 *
	 * ⚠ 값이 전부 0 이면 어트리뷰트도 0 이 된다. MoveSpeed 0 은 하한으로 잘린다.
	 * 레벨 성장은 F10 이 별도 GE 로 처리한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	FERCharStats BaseStats;
};
