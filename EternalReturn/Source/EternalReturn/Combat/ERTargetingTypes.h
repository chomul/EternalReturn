// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ERTargetingTypes.generated.h"

/** 판정 형상. 6인이 요구하는 것 전부다. */
UENUM(BlueprintType)
enum class ESkillTargeting : uint8
{
	/** 지정한 액터 하나 */
	SingleTarget,
	/** 시전자 중심 원형 */
	SelfRadius,
	/** 방향 직선. bPenetrate 로 관통 여부를 가른다 */
	Projectile,
	/** 지정한 좌표 중심 원형 */
	GroundCircle,
	/** 부채꼴 */
	Cone,
	/** ⭐ 이중 반경 — 중앙과 외곽을 다른 결과로 돌려준다 (레니 W) */
	DualRadius,
};

/** 누구를 걸러낼 것인가. */
UENUM(BlueprintType)
enum class ETargetTeamFilter : uint8
{
	/** 적만. ⚠ 팀 없는 액터(야생동물)도 여기 포함된다 — 안 그러면 곰을 못 때린다 */
	Enemy,
	/** 아군만. 팀 없는 액터는 제외된다 */
	Ally,
	/** 전부 */
	All,
};

/**
 * 판정 1회에 필요한 전부.
 *
 * ⚠ 길이 단위는 전부 **미터(m)** 다. 언리얼 내부 단위(uu)로의 변환은
 *   ERTargeting 안 한 곳에서만 일어난다.
 *
 * ⚠ 사거리 클램프는 이 구조체를 채우기 **전에** 끝나 있어야 한다.
 *   클라가 보낸 조준 좌표를 서버가 클램프하는 것은 스킬 시스템의 책임이고,
 *   여기서는 이미 클램프된 값을 받는다고 전제한다. 안 그러면 사거리 핵이 된다.
 */
USTRUCT()
struct FTargetQuery
{
	GENERATED_BODY()

	UPROPERTY()
	ESkillTargeting Shape = ESkillTargeting::SingleTarget;

	UPROPERTY()
	ETargetTeamFilter TeamFilter = ETargetTeamFilter::Enemy;

	/** 판정의 기준점. 시전자 발밑이거나 지정한 좌표다. */
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	/** Projectile / Cone 이 쓰는 방향. 정규화되지 않아도 된다 — 내부에서 정규화한다. */
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	/** 사거리 상한 (m) */
	UPROPERTY()
	float RangeMax = 0.f;

	/** ⭐ 사거리 하한 (m). 시셀라 Q 는 1.0m 안쪽이면 안 나간다. */
	UPROPERTY()
	float RangeMin = 0.f;

	/** DualRadius 의 중앙 반경 (m). 레니 W = 1.25 */
	UPROPERTY()
	float RadiusInner = 0.f;

	/** DualRadius 의 외곽 반경 (m). 레니 W = 2.25 */
	UPROPERTY()
	float RadiusOuter = 0.f;

	/** ⭐ 부채꼴 **전체** 각도(도). 다니엘 Q = 65 → 좌우 각 32.5도. 반각이 아니다. */
	UPROPERTY()
	float AngleDeg = 0.f;

	/** Projectile 의 두께(반지름, m). 0 이면 얇은 선이 되어 실전에서 잘 안 맞는다. */
	UPROPERTY()
	float ProjectileRadius = 0.25f;

	/** 관통 여부. false 면 가장 가까운 하나만. */
	UPROPERTY()
	bool bPenetrate = false;

	/**
	 * ⭐ Z를 무시하고 수평(XY) 거리로만 판정할 것인가. 기본 true.
	 *
	 * 탑다운이라 수평 판정이 직관과 맞는다. 높이차가 큰 지형에서 문제가 되면
	 * 스킬 단위로 끌 수 있게 플래그로 뺐다 (Docs/Argument/2 B-5).
	 */
	UPROPERTY()
	bool bIgnoreZ = true;

	/**
	 * 판정을 일으킨 액터. 팀 필터와 자기 자신 제외에 쓴다.
	 *
	 * ⚠ 팀 판정을 여기서 하지 않는다 — ERTeamStatics 가 유일한 팀 로직 소유자다.
	 */
	UPROPERTY()
	TObjectPtr<const AActor> Instigator = nullptr;

	/** SingleTarget 이 지정하는 대상. 다른 형상에서는 무시된다. */
	UPROPERTY()
	TObjectPtr<AActor> DesignatedTarget = nullptr;

	/** 결과에서 뺄 액터들. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> IgnoredActors;

	/** 시전자 본인을 결과에 포함할 것인가. 시셀라 R 처럼 자기를 대상으로 하는 스킬이 있다. */
	UPROPERTY()
	bool bIncludeInstigator = false;
};

/**
 * 판정 결과.
 *
 * ⚠ 배열은 전부 **가까운 순**으로 정렬돼 있다. 카티야 R 이 "가까운 순 3발"을 요구한다.
 */
USTRUCT()
struct FTargetResult
{
	GENERATED_BODY()

	/** 맞은 액터들. DualRadius 에서는 **외곽만** 담긴다. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> HitActors;

	/** ⭐ DualRadius 의 중앙에 맞은 액터들. 다른 형상에서는 비어 있다. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> InnerHitActors;

	/**
	 * HitActors 와 같은 순서의 거리(m).
	 *
	 * 거리 비례 보간(카티야 Q)이 이 값을 쓴다. 호출자가 거리를 다시 재면
	 * 그 사이에 대상이 움직여 판정 시점과 값이 달라진다.
	 */
	UPROPERTY()
	TArray<float> Distances;

	bool IsEmpty() const { return HitActors.IsEmpty() && InnerHitActors.IsEmpty(); }
};
