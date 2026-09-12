// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ACharacter;

/**
 * 강제 이동 (넉백 · 그랩).
 *
 * ⭐ **넉백과 그랩은 같은 함수다.** 방향 부호만 다르다 —
 *   역기획서 §3.5 가 "같은 함수에 방향 인자를 받는다" 로 명시한다.
 *
 * ⭐⭐ **왜 RootMotionSource 인가** (Docs/4_Argument/13_강제이동_구현방식.md 방안 B)
 *   강제 이동은 유일하게 캐릭터 위치를 바꾸는 CC 라, 서버가 위치만 바꾸면
 *   클라가 되돌리려 하면서 러버밴딩이 난다(역기획서 §3.5).
 *
 *   RootMotionSource 는 그 문제를 엔진이 이미 풀어 둔 자리다:
 *     - FSavedMove_Character 에 SavedRootMotion 이 있다 (CharacterMovementComponent.h:2945)
 *       -> 클라 예측이 ServerMove 에 실린다 (E08 에서 RequestedVelocity 가 없어서 겪은 문제가 없다)
 *     - 서버 교정 RPC 가 내장돼 있다 (:2481, :2569)
 *     - 콜리전·지형 처리를 CMC 가 그대로 한다
 *
 * ⚠⚠ **다만 CurrentRootMotion 은 복제되지 않는다** (:2677, Transient).
 *   서버에서만 추가하면 클라는 모른다. 그래서 **NetMulticast 로 파라미터를 뿌려
 *   각 머신이 같은 소스를 자기 CMC 에 추가**한다.
 *   (GAS 태스크는 어빌리티가 양쪽에서 실행되는 것을 전제로 같은 일을 한다 —
 *    AbilityTask_ApplyRootMotionMoveToForce.cpp:77. 우리는 어빌리티가 없어서 직접 한다)
 */
namespace ERForcedMove
{
	/** 이 이름으로 소스를 넣고 찾는다. 중단할 때 필요하다. */
	extern const FName ForceName;

	/**
	 * [서버] 강제 이동을 시작한다.
	 *
	 * @param Target     밀려날 캐릭터
	 * @param Direction  월드 방향. ⭐ **정규화된다.** 넉백은 시전자→대상, 그랩은 반대
	 * @param DistanceUU 거리(cm). ⚠ 역기획서는 m 로 적혀 있다 — 부르는 쪽에서 환산한다
	 * @param Duration   걸리는 시간(초)
	 * @return 시작했으면 true
	 *
	 * ⚠ 서버에서만 부른다. 실패하면 로그를 남긴다.
	 */
	bool ApplyForcedMove(ACharacter* Target, const FVector& Direction, float DistanceUU, float Duration);

	/**
	 * [모든 머신] 실제로 RootMotionSource 를 붙인다. **직접 부르지 않는다** —
	 * ApplyForcedMove 가 서버에서, Multicast RPC 가 각 클라에서 부른다.
	 */
	void AddForcedMoveSource(ACharacter* Target, const FVector& StartLocation,
		const FVector& TargetLocation, float Duration);

	/**
	 * [모든 머신] 강제 이동을 중단한다. 벽에 부딪혔을 때 서버가 부르고 Multicast 로 전파한다.
	 */
	void RemoveForcedMoveSource(ACharacter* Target);

	/**
	 * [서버] 강제 이동 중인가.
	 *
	 * ⚠ 벽 충돌 감시를 언제 멈출지 판단하는 데 쓴다.
	 */
	bool IsForcedMoving(const ACharacter* Target);

	/**
	 * [서버] 벽에 부딪혔는지 한 번 검사한다.
	 *
	 * ⭐⭐ **서버에서만 부른다.** 클라가 판정하면 머신마다 결과가 갈린다
	 *   (역기획서 §3.5 — "벽 충돌은 서버에서만 판정").
	 *
	 * ⚠ 이 함수는 **판정만 한다.** 추가 피해·기절은 부르는 쪽(스킬, F07)이 정한다 —
	 *   매그너스 E 와 레니 R 의 피해량이 다르기 때문이다.
	 *
	 * @param OutHit  부딪혔으면 충돌 정보
	 * @return 벽에 부딪혔으면 true
	 */
	bool CheckWallImpact(ACharacter* Target, FHitResult& OutHit);
}
