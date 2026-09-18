// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GAS/ERAttributeTypes.h"
#include "Item/ERItemTypes.h"

class UERSkillData;

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
	 * 레벨 성장은 아래 Growth 로 F10 이 별도 GE 로 처리한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	FERCharStats BaseStats;

	/**
	 * 이 실험체의 스킬. P / Q / W / E / R.
	 *
	 * ⭐ **하드 참조다.** 실험체를 로드하면 스킬은 반드시 필요하고 따로 언로드할 일이 없다
	 *   (Docs/6_Lyra참조/03 §5 의 기준 — "따로 로드/언로드할 일이 있나? 없으면 하드").
	 *
	 * ⚠ **D 슬롯은 여기 없다.** 무기가 소유한다 (역기획서 §1.2). F11 에서 무기 데이터에 붙는다.
	 *
	 * ⚠ 순서에 의미가 없다. 슬롯은 각 UERSkillData 의 SlotTag 가 정한다.
	 *   같은 슬롯이 둘 있으면 둘 다 부여되고 입력이 둘 다 발동시킨다 — 애셋 실수다.
	 *   GrantSkills 가 검사한다.
	 *
	 * 근거: Docs/4_Argument/15_스킬데이터_위치.md (방안 B)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "스킬")
	TArray<TObjectPtr<UERSkillData>> Skills;

	/**
	 * 이 실험체가 들 수 있는 무기군 (F08-02). 무기 역기획서 §5.1 — 카티야 [SniperRifle] · 재키 [Dagger, TwoHandSword, Axe, DualSword] …
	 *
	 * ⭐ "무기가 캐릭터를 고른다" (장비 역기획서 §2) — 장착 검사는 ERItem::CanEquip 이 이 배열을 본다. **서버가 검사한다.**
	 * ⚠ 비어 있으면 어떤 무기도 못 든다 — 방어구는 제한이 없으니 영향 없음. 6인 밖 실험체는 (미확인).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "무기")
	TArray<EERWeaponType> WeaponTypes;

	/** 레벨당 스탯 증가 (F10-02). 레벨업마다 UERLevelUpEffect(Instant) 로 BaseStats 위에 더해진다. */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	FERCharStatGrowth Growth;

	/**
	 * 무기군별 숙련도 증폭 계수 (F10-04). 키는 WeaponTypes 와 같은 집합이어야 한다 — 들 수 있는데 여기 없으면 계수 0 + Warning.
	 * 테이블이 아니라 여기인 이유: 실험체를 식별할 키가 애셋뿐이다 (Docs/4_Argument/23_숙련도증폭계수_위치.md 방안 B).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "무기")
	TMap<EERWeaponType, FERWeaponAmp> WeaponProficiencyAmp;
};
