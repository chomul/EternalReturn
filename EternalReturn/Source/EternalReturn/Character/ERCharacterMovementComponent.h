// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ERCharacterMovementComponent.generated.h"

/**
 * 이동 차단을 담당하는 CMC.
 *
 * ⭐ **이동 차단의 유일한 지점이다.** 기절·속박·붙잡기 등 어떤 CC 든
 *   State.Block.Movement 태그를 부여하면 여기서 막힌다.
 *   CC 종류를 모른다 — 그래서 CC 를 추가해도 이 파일을 고치지 않는다.
 *
 * ⭐ **왜 여기가 옳은가 (네트워크)**
 *   GetMaxSpeed() 는 CalcVelocity() 안에서 호출된다
 *   (CharacterMovementComponent.cpp:3641 -> :3651).
 *   이 경로는 **클라 예측 · 서버 재생 · 서버 보정 후 리플레이가 전부 같이 탄다.**
 *   태그는 ASC 가 복제하므로 양쪽이 같은 값을 보고 **같은 결과**를 낸다.
 *   → 이동 차단용 RPC 가 필요 없다. 만들면 오히려 어긋난다.
 *
 * ⚠ **AddLooseGameplayTag 로 CC 를 걸면 안 된다.** 복제되지 않아서
 *   서버만 막히고 클라는 계속 움직인다 — 고무줄의 교과서적 원인이다.
 *
 * 참조: Lyra 의 ULyraCharacterMovementComponent (같은 패턴, 태그 1개)
 *      Docs/6_Lyra참조/04_이동차단.md
 * 근거: Docs/4_Argument/10_CC_차단축_태그설계.md (방안 A)
 */
UCLASS()
class UERCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UERCharacterMovementComponent();

	/** State.Block.Movement 가 붙어 있으면 0. 그 외에는 기본 동작. */
	virtual float GetMaxSpeed() const override;

	/**
	 * 이동이 막힌 동안에는 회전도 막는다.
	 *
	 * ⚠ 속도만 막으면 **제자리에서 방향은 계속 돌아간다.** 기절 중에 조준을
	 *   바꿀 수 있게 되므로 실제로 문제가 된다. Lyra 도 두 함수를 함께 막는다
	 *   (LyraCharacterMovementComponent.cpp:106-130).
	 */
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;

private:
	/**
	 * 이동이 막혀 있는가. 두 오버라이드가 공유한다.
	 *
	 * ⚠ 매 프레임 여러 번 ASC 를 조회한다. Lyra 도 같다.
	 *   **지금 캐시하지 않는다** — 24명 + 야생동물 규모에서 실제로 문제인지
	 *   측정하기 전에는 알 수 없고, 추측으로 넣은 캐시는 태그 변화를 놓친다.
	 *   필요해지면 ASC->RegisterGameplayTagEvent 로 bool 을 유지하는 것이
	 *   올바른 방향이다 (태그가 바뀔 때만 갱신된다).
	 */
	bool IsMovementBlocked() const;
};
