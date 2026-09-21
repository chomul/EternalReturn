// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/ERItemTypes.h"

struct FERWeaponClassRow;

/** 무기 계열 조회 (F11-01). 테이블만 읽는다. */
namespace ERWeapon
{
	/**
	 * 계열 행. 없으면 nullptr + **Error** (조용히 "D 없음" 이 되면 못 찾는다 — 23종 중 행이 없는 계열을 장착한 경우).
	 * 행의 DSkill / AttackData 가 있으면 SlotTag 를 검사한다 — 틀리면 Error (행은 돌려준다).
	 */
	const FERWeaponClassRow* Find(EERWeaponType WeaponType);

	/** 전 행 검사 — 시작 때 한 번 (WeaponType 과 RowName 불일치 · 중복 · SlotTag). 틀린 줄마다 Error. */
	bool ValidateTable();
}
