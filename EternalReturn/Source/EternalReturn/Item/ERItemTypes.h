// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ERItemTypes.generated.h"

/**
 * 아이템 등급. 장비 역기획서 §1 순서 그대로.
 *
 * ⚠⚠ **순서를 바꾸지 않는다.** 데이터 테이블 · 애셋에 정수로 저장되고, 제작 트리(F09)와 스탯 배율 양쪽이 이 순서를 쓴다.
 *   새 등급이 생기면 **끝에** 붙인다.
 */
UENUM(BlueprintType)
enum class EERItemGrade : uint8
{
	/** 일반 — 상자 · 채집 · 야생동물 · 원격 드론 */
	Common,
	/** 고급 — 제작 */
	Uncommon,
	/** 희귀 — 제작 / 상자 (모터 · 아이테르 깃털 등 특수 재료) */
	Rare,
	/** 영웅 — 제작 / 영웅 보급 상자 */
	Hero,
	/** 전설 — 제작 / 항공 보급. 대부분의 최종 목표 */
	Legend,
	/** 초월 — 초월 보급 · 전투 실험실 · 코발트 합성기. 방어구 4종 중 1택 */
	Transcendent,
};

/** 장비 슬롯. 장비 역기획서 §2 — 무기 1 + 방어구 4. ⚠ 순서 고정. */
UENUM(BlueprintType)
enum class EEREquipSlot : uint8
{
	/** 장비가 아님 (재료 · 소모품). 인벤토리에는 있되 장착 칸이 없다 */
	None,
	/** ⭐ 해당 무기군을 보유한 실험체만 장착 (F08-02) */
	Weapon,
	/** 머리 */
	Head,
	/** 옷 */
	Chest,
	/** 팔 / 장식 */
	Arm,
	/** 다리 */
	Leg,
};

/**
 * 무기 계열 23종. 무기계열_D스킬 역기획서 §2 · §4.3 — "23종 전부 자리를 만들되, 초기 데이터 행은 8개만".
 * 장착 제한(F08-02)과 D스킬(F11)이 이 값을 공유한다. ⚠ 순서 고정.
 */
UENUM(BlueprintType)
enum class EERWeaponType : uint8
{
	None,
	// ── 근접 13 ── (주석은 UHT 툴팁이 된다 — 앞줄 /** */ 만 그 값에 붙는다)
	/** 글러브 */ Glove,
	/** 톤파 */ Tonfa,
	/** 방망이 */ Bat,
	/** 망치 */ Hammer,
	/** 도끼 */ Axe,
	/** 단검 */ Dagger,
	/** 양손검 */ TwoHandSword,
	/** 쌍검 */ DualSword,
	/** 창 */ Spear,
	/** 쌍절곤 */ Nunchaku,
	/** 레이피어 */ Rapier,
	/** 채찍 */ Whip,
	/** VF 의수 */ VFArm,
	// ── 원거리 6 ──
	/** 권총 */ Pistol,
	/** 돌격소총 */ AssaultRifle,
	/** 저격총 */ SniperRifle,
	/** 활 */ Bow,
	/** 석궁 */ Crossbow,
	/** 암기 */ Shuriken,
	// ── 특수 4 ──
	/** 투척 */ Throw,
	/** 기타(악기) */ Guitar,
	/** 카메라 */ Camera,
	/** 아르카나 */ Arcana,
};
