// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ERWildlifeTypes.generated.h"

/**
 * 야생동물 종류 (F12-01 · 역기획서 §1 · §5). 종 정의는 UERWildlifeData 애셋 (Argument 30 B) — 변이체도 **별도 애셋** (사용자 2026-09-21). 이 값은 분류 · 드랍 분기용.
 */
UENUM()
enum class EERWildlifeType : uint8
{
	None,
	/** 닭 — 티어 1 · 깃털 확률 드랍 */
	Chicken,
	/** 박쥐 — 티어 2 · 가죽 없음 (12.0) */
	Bat,
	/** 멧돼지 — 티어 2 */
	Boar,
	/** 까마귀 — 가죽 없음 */
	Crow,
	/** 들개 — 티어 3 */
	WildDog,
	/** 늑대 — 티어 3 · 1일차 낮 1분부터 */
	Wolf,
	/** 곰 — 티어 4 · 기절기 · 1일차 밤부터 */
	Bear,
	/** 드론 — 배터리 확정 */
	Drone,
	/** 보스 — 알파 (2일차 밤 · 미스릴) */
	Alpha,
	/** 보스 — 오메가 (3일차 밤 · 포스코어) */
	Omega,
	/** 보스 — 위클라인 (4일차 밤 · 비례 피해 50%) */
	Wickeline,
};

/** 변이 단계 (F12-03). 크레딧이 단계마다 다르다 — 닭 1/2/3 · 곰 5/10/20 (역기획서 §1.2 [확인]). 종 정의는 단계마다 **별도 애셋**. */
UENUM()
enum class EERWildlifeVariant : uint8
{
	Normal,
	/** 변이체 */
	Mutant,
	/** 잠식 변이체 — 닭 · 들개 · 늑대 · 곰만 있다 */
	Corrupted,
};

/** 재생성 방식 (F12-03 · 역기획서 §3.3 [확인]). 기준점은 **처치 순간** (사용자 2026-09-23). */
UENUM()
enum class EERWildlifeRespawn : uint8
{
	/** 처치 후 RespawnSeconds 뒤 (닭 120 · 박쥐 150 · 멧돼지 130 · 들개 140 · 드론 210) */
	Interval,
	/** 다음 낮이 시작될 때 (늑대) */
	EveryDay,
	/** 다음 밤이 시작될 때 (곰) */
	EveryNight,
	/** 한 번만 (보스 — F12-05) */
	Never,
};
