// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Item/ERItemTypes.h"
#include "ERWeaponTypes.generated.h"

class UERSkillData;

/**
 * 무기 계열 한 행 (F11-01). RowName = 계열명 ("Hammer") — EERWeaponType 과 1:1. 초기 데이터는 8행 (역기획서 §4.2), enum 자리는 23.
 *
 * ⭐ **무기가 D 와 기본 공격을 소유한다.** 실험체 애셋에는 D 가 없다 (F07-01). 장착 → 부여, 해제 → 회수는 F11-02.
 * ⭐ D 강화 0/1/2 = 스킬 레벨 1/2/3 — UERSkillData 의 레벨별 배열이 그대로 강화 단계. 별도 강화 구조 없음.
 * ⭐ `FCharacterWeaponRow`(실험체 × 무기 허용)는 만들지 않는다 — UERCharacterData.WeaponTypes 가 이미 그 표다 (F08-02).
 * ⚠ 5 / 10 / 15 는 행 값. 코드에 박지 않는다.
 */
USTRUCT(BlueprintType)
struct FERWeaponClassRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EERWeaponType WeaponType = EERWeaponType::None;

	/** 계열 D 스킬. SlotTag 는 Ability.Slot.D · bUsesSkillPoints false · InitialLevel 0 · MaxLevel 3 이어야 한다 — ERWeapon::Find 가 검사. 비면 D 없음 (05 전까지). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UERSkillData> DSkill;

	/** 계열 기본 공격 데이터. SlotTag 는 Ability.Slot.Attack. 비면 실험체 기본 평타 유지 (F07-07). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UERSkillData> AttackData;

	/** 기본 공격 사거리 (m). 장착 시 AttackRange BaseValue 를 이 값으로 **교체** (Docs/4_Argument/24 방안 B). ⚠ 자체 결정값 — 원작 (미확인). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float AttackRange = 2.f;

	/** D 해금 숙련도 레벨. [확인] 5 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 UnlockLevel = 5;

	/** D 강화 숙련도 레벨 — [i] 에 닿으면 스킬 레벨 +1. [확인] {10, 15} */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<int32> UpgradeLevels = { 10, 15 };
};
