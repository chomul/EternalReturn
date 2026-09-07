// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ERTargetingTypes.h"

class UWorld;

/**
 * "어느 위치에 어떤 모양을 그렸을 때 누가 맞았는가" 에만 답하는 순수 함수 묶음.
 *
 * 이 클래스가 하지 않는 것:
 *   - 피해를 주지 않는다 (데미지는 별도 계산 경로)
 *   - 액터를 스폰하지 않는다 (날아가는 투사체는 스킬 시스템 소관)
 *   - 사거리를 클램프하지 않는다 (서버 RPC 수신부 소관 — 안 하면 사거리 핵)
 *   - 상태를 갖지 않는다 (컴포넌트가 아니라 정적 함수다)
 *
 * ⭐ GAS 는 판정 형상을 계산해주지 않는다. 부채꼴·이중 반경·관통은 직접 만들어야 하고,
 *   GAS 가 주는 것은 결과를 담는 그릇(FGameplayAbilityTargetData)뿐이다.
 *   그 변환은 스킬 쪽에서 한다 — 여기 넣으면 이 파일이 GAS 에 묶인다.
 */
class ERTargeting
{
public:
	/** Shape 에 따라 아래 함수들로 분기한다. 스킬은 보통 이것만 부르면 된다. */
	static FTargetResult Query(const UWorld* World, const FTargetQuery& Q);

	/** 지정한 액터 하나가 사거리·필터를 통과하는지 */
	static FTargetResult QuerySingleTarget(const UWorld* World, const FTargetQuery& Q);

	/** Origin 중심 원형 */
	static FTargetResult QuerySelfRadius(const UWorld* World, const FTargetQuery& Q);

	/** 방향 직선. bPenetrate 가 false 면 가장 가까운 하나만 */
	static FTargetResult QueryProjectile(const UWorld* World, const FTargetQuery& Q);

	/** 지정한 좌표 중심 원형. SelfRadius 와 계산은 같고 의미가 다르다 */
	static FTargetResult QueryGroundCircle(const UWorld* World, const FTargetQuery& Q);

	/** 부채꼴. AngleDeg 는 전체 각도다 */
	static FTargetResult QueryCone(const UWorld* World, const FTargetQuery& Q);

	/** ⭐ 이중 반경. 중앙은 InnerHitActors, 외곽은 HitActors 로 나뉜다 (중복 없음) */
	static FTargetResult QueryDualRadius(const UWorld* World, const FTargetQuery& Q);

	/**
	 * 사거리 하한~상한 사이에서 0.0~1.0 을 돌려준다.
	 *
	 * 보간은 하지 않는다 — 알파만 준다. 카티야 Q 는 기본 피해와 계수를
	 * **각각** 보간하므로, 여기서 하나로 합치면 쓸 수 없다.
	 */
	static float GetDistanceAlpha(float Distance, float RangeMin, float RangeMax);

	/**
	 * 판정 기준점. 캡슐이 있으면 **발밑**, 없으면 액터 위치.
	 *
	 * ⭐ ACharacter 는 캡슐이 루트라 GetActorLocation() 이 캡슐 **중심**을 준다.
	 *   그대로 쓰면 키가 다른 대상(곰 vs 닭)끼리 거리가 어긋난다.
	 *   발밑을 쓰면 이동 시스템(GetNavAgentLocation)과 기준이 같아진다.
	 */
	static FVector GetTargetingLocation(const AActor* Actor);
};
