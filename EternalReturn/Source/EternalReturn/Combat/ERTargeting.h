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

	/**
	 * 날아가는 투사체 한 틱 구간 From → To 를 반경으로 쓸어 **필터를 통과한 대상**을 가까운 순으로 (F19-01 · Argument 54).
	 * 판정 필터(팀 · ASC · 시체 · 무시 목록)는 다른 모양과 **같은 함수** — Q.TeamFilter · Q.Instigator · Q.IgnoredActors 를 쓴다.
	 */
	static TArray<AActor*> SweepSegment(const UWorld* World, const FVector& From, const FVector& To, float RadiusUU, const FTargetQuery& Q);

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

	/** 시전자 앞 역사다리꼴 (카티야 R) — 가까운 순 · MaxTargets */
	static FTargetResult QueryTrapezoid(const UWorld* World, const FTargetQuery& Q);

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

	/**
	 * From 에서 대상 **판정면까지**의 거리 (uu). 콜리전이 없으면 중심(발밑) 거리로 되돌아간다.
	 *
	 * ⭐ 중심 기준은 몸이 큰 대상에서 무너진다 — 늑대(앞뒤 2.9m)는 배에 붙어도 중심까지 2.2m 라 망치(1.5m)가 빗나갔다
	 *   (2026-09-22 로그 · Argument 32). SkillTarget 을 막는 프리미티브(야생동물 = HitBox · 실험체 = 캡슐)의 표면을 쓴다.
	 * ⚠ 시전자 쪽은 아직 중심이다 — 몸이 큰 야생동물이 때리는 쪽은 F12-04 에서 (Argument 32).
	 */
	static float DistanceToSurface(const AActor* Target, const FVector& From, bool bIgnoreZ);

	/**
	 * SingleTarget 사거리에 쓰는 거리 (uu) — 대상 쪽은 항상 표면, **시전자 몸이 캡슐보다 크면 시전자 표면에서도** 잰다 (Argument 32).
	 * "시전자 몸이 크다" = SkillTarget 에 응답하는 컴포넌트가 루트(캡슐)가 아니다 → 야생동물 HitBox. 실험체는 캡슐이 루트라 그대로다.
	 * ⭐ AI 이동 판단(F12-04)과 어빌리티 판정이 **같은 함수**를 쓴다 — 다르면 "붙었는데 못 때림 / 못 붙었는데 멈춤" 이 번갈아 난다.
	 */
	static float SingleTargetDistance(const AActor* Instigator, const FVector& Origin, const AActor* Target, bool bIgnoreZ);

	/** 판정을 받는 컴포넌트 (SkillTarget 에 Block 또는 Overlap). 야생동물 = HitBox · 실험체 = 캡슐. 없으면 nullptr. */
	static const class UPrimitiveComponent* FindSkillShape(const AActor* Actor);
};
